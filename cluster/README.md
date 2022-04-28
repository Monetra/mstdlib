# Cluster Implementation

Inspired by the RAFT Consensus algorithm by Diego Ongaro:
[Consensus: Bridging Theory and Practice](https://web.stanford.edu/~ouster/cgi-bin/papers/OngaroPhD.pdf)

Also took into consideration modification recommendations by Henrik Ingo:
[Four modifications for the Raft consensus algorithm](https://www.openlife.cc/sites/default/files/php_uploads/4-modifications-for-Raft-consensus.pdf)

Though this implementation doesn't strictly conform to the above, it attempts
to in spirit along with some real world modifications to better handle
potentially hostile environments.

## Overview

The cluster system relies on a single elected Leader in order to make
modifications.  Each client modification request sends the log entry to the
leader only, the leader then adds a log entry to itself, then replicates the
entry to all connected follower nodes.  When a Quorum has been reached (> 50%
of nodes have acknowledged the entry), the response is sent to the client.

Every member of the cluster is always attached to every other member of the
cluster (star topology) and each node will be required to handle heartbeat
messages.

All clients of the system must be cluster members in either the Leader or
Follower state as the peer connections are used for all messaging.

The log entry system is designed to be pluggable.  It is up to the integrator
to provide the plugin implementation to process these log messages.

The design tries to automate as much of the cluster configuration and setup
as possible by closely tracking node response times and automatically adjusting
fault and election timers as network conditions change.  The entire cluster can
be initialized from a blank state once at least a quorum of configured and
authenticated servers is reached.  A new node can join the cluster and sync
all data from the cluster as long as it can reach the Leader.


## Plug-in Callback System

The plugin system is best described using an example.

A common implementation of a plugin might consist of common actions such as:

   ReadKey, InsertKey, InsertOrReplaceKey, CompareAndSetKey, IncrementKey,
   DecrementKey.

Some of the above list of actions may have validators associated with them
which can fail (For instance, CompareAndSet may fail if the required value
doesn't match the current value).  If the validation by the plugin fails, the
entire request is rejected and no replication attempt is made.

The plugin may also choose to rewrite the log entry. For instance, the other
nodes have no need to perform an action on ReadKey, so a NoOp log entry might be
output instead.  Though, a NoOp entry is still replicated to provide
consistency guarantees (which may be invalidated if the current Leader isn't
really the Leader any more). Similarly, log entires like InsertKey,
CompareAndSetKey, IncrementKey, and DecrementKey might all be rewritten to a
simplified InsertOrReplaceKey as an optimization to prevent the Follower nodes
from needing to do additional work.

If the validation step of the plugin fails for Follower receiving the replicated
log, the node will immediately go into an Error state and notify all connected
peers. This most likely means the node is corrupt.

In addition to the above log insertion/interpretation, the plug-in system must
also support serialization and deserialization of the data (regardless if
stored on disk or in memory) for bringing new nodes online.

An optional callback can also be called to rollback log entries.  Most
implementations probably won't have this ability.  This would only be called
in the event that a node receives a log entry from the leader and applies it,
but then the leader crashes before reaching quorum or isn't really the leader
anymore but neither itself nor the follower that accepted the log entry know
this; *and* the node which received the log entry doesn't win the election to be
the new leader (possibly due to a network split).  If the callback is registered
it will be used to rollback entries which the rest of the network does not know
about, otherwise the node will be forced to zero out its data set and redownload
it in its entirety from the new leader.


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
              (e.g. reboot).  For tracking connected peers, once this step is
              finished (peer is disconnected), if the node exists in
              the global system configuration, it will go into the INIT state,
              otherwise will be removed from the server list completely.
- Term          - u64  - Last known node term, 0 if not set
- LogID         - u64  - Last known log id, 0 if not set
- Log[]         - list - Committed Log Entries
- AvgLatencyMs  - u64  - The average latency in ms for the cluster
- ClusterID     - u64  - Unique cluster state id when a new cluster is brought
                         online.  This prevents 2 detached clusters using the
                         same nodes and configuration from trying to merge.
- Servers[]     - list - Linked List of tracked server objects containing the
                         peer node ip address, port, state, statistics and so
                         on.
- VotedFor      - ptr   - Points to the server voted in the current term if any
- ElectionTimer - timer - Timer to try start a new election.  Reset if node
                          receives a RequestVote


### Servers List

The list of servers maintained in the node state contains these data elements:

- Ip Address     - text  - Ip address of peer
- Port           - u16   - Peer Port
- State          - enum  - Any of the same states as our own Node State, but
                           specific to the peer.
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
- HeartbeatTimer - timer - Timer to send heartbeat messages which fire off every
                           2x AvgLatencyMs in node state.


### Calculated Node Variables

- AvgLatencyMs -      Received during Vote for leader from proposed leader.  If
                      proposing, perform MAX(SUM(Servers[n].Latency)) and use
                      that value (minimum 1).
- ElectionTimeoutMs - Base value is 10 x AvgLatencyMs with a minimum value of
                      100ms.  An existing leader node will use this value as its
                      election timer.  Follower nodes will use a random value of
                      1.5x to 2x the base value.
- FaultTimeoutMs -    25x AvgLatencyMs or Configured Maximum RTT, whichever is
                      less.  If a reply to a message takes longer than this, the
                      node is put into an Error state and disconnected.

