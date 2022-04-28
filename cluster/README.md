# Cluster Information

Inspired by the RAFT Consensus algorithm by Diego Ongaro:
[Consensus: Bridging Theory and Practice](https://web.stanford.edu/~ouster/cgi-bin/papers/OngaroPhD.pdf)

And modification recommendations by Henrik Ingo:
[Four modifications for the Raft consensus algorithm](https://www.openlife.cc/sites/default/files/php_uploads/4-modifications-for-Raft-consensus.pdf)

Though this implementation doesn't strictly conform to the above, it attempts
to in spirit along with some real world modifications to better handle
potentially hostile environments.


## System Configuration

- [R] Cluster Name     - Name of the cluster to use when joining other nodes
                         (failure if mismatch)
- [R] SharedSecret     - Shared Secret for cluster authentication.  Will be
                         authenticated using HMAC using a peer-provided nonce to
                         prevent replay attacks
- [R] Server List      - List of servers (with port) to attempt to connect to as
                         part of the cluster (does not have to be complete, but
                         should be).  Must all be from the same address class
                         (ipv4 vs ipv6)
- [R] Flags            - VOTE_ONLY (can only vote, does not actually receive
                         logs, essentially arbitrator mode), TLS_NOVERIFY_PEER
                         (don't verify the peer certificate)
- [R] Maximum RTT      - Maximum RTT response time in ms from any node before
                         evicting the node in an error state. This should be
                         at least 10x the average round trip time to handle
                         any packet rerouting due to interface issues.
- [R] Maximum Log Size - Maximum size in bytes of the recorded log payloads.
                         Will purge the oldest entries until it reaches the
                         desired cumulative size.  This affects syncing after a
                         detached member rejoins the cluster, if older logs
                         have been purged, will require a full data
                         serialization and transfer.
- [R] Port             - Port to listen on
- [O] Node IP Address  - IP Address of local machine to use to connect to peers.
                         Must be of the same address class as the Server List.
                         If not provided, will choose a non-localhost and
                         non-link-local ip address at random.

## Node State

- State         - enum:
  - INIT:     Initializing - may not be connected to any nodes, this is the
              initial state.  When tracking servers, this state does not count
              toward quorum.
  - LEADER:   Primary Node.  Responsible for processing all requests.
  - FOLLOWER: Not the primary node, just receiving logs from leader to apply and
              can participate in voting.
  - VOTER:    Does not receive logs, only participates in voting (arbitration
              mode)
  - ERROR:    Node is in an error state (e.g. maximum latency, non-leader
              sending log entries), evicted from cluster.  Still counts toward
              quorum as it is expected to reconnect and re-sync.
  - FINISH:   Node is in the process of detaching from the cluster
              (e.g. reboot), once this step is finished, if the node exists in
              the global system configuration, it will go into the INIT state,
              otherwise will be removed from the server list completely.
- Term          - u64  - Last known node term, 0 if not set
- LogID         - u64  - Last known log id, 0 if not set
- AvgLatencyMs  - u64  - The average latency in ms for the cluster
- ClusterID     - u64  - Unique cluster state id when a new cluster is brought
                         online.  This prevents 2 detached clusters using the
                         same nodes and configuration from trying to merge.
- Servers[]     - list - Linked List of tracked server objects containing the
                         peer node ip address, port, state, statistics and so
                         on.
- VotedFor      - ptr   - Points to the server voted in the current term if any
- ElectionTimer - timer - Timer to start a new election.


### Servers List

The list of servers maintained in the node state contains these data elements:

- Ip Address     - text  - Ip address of peer
- Port           - u16   - Peer Port
- State          - enum  - Any of the same states as self
- ConnectTimer   - et    - Elapsed Time since connect. Monotonic tracking of how
                           long this node has been connected.
- LastMsgTimer   - et    - Elapste Time since last message. Monotonic tracking
                           of last message time to detect lagged nodes and
                           heartbeats.
- Latency[]      - u64[] - Circular Array of last 1024 response latencies in ms
- Term           - u64   - Last known term of node
- LogID          - u64   - Last known LogID of node
- ConnectedPeers - u16   - Known number of connected peers that are in a leader,
                           follower, or voter state.


### Calculated

- AvgLatencyMs - Received during Vote for leader from proposed leader.  If
                 proposing, perform MAX(SUM(Servers[n].Latency)) and use that
                 value (minimum 1).
- ElectionTimeout - Base value is 10 x AvgLatencyMs with a minimum value of
                    100ms.  An existing leader node will use this value as its
                    election timer.  Follower nodes will use a random value of
                    1.5x to 2x the base value.

