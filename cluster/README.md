# Cluster Implementation

Inspired by the RAFT Consensus algorithm by Diego Ongaro:
[Consensus: Bridging Theory and Practice](https://web.stanford.edu/~ouster/cgi-bin/papers/OngaroPhD.pdf)

Also took into consideration modification recommendations by Henrik Ingo:
[Four modifications for the Raft consensus algorithm](https://www.openlife.cc/sites/default/files/php_uploads/4-modifications-for-Raft-consensus.pdf)

Though this implementation doesn't strictly conform to the above, it attempts
to in spirit along with some real world modifications to better handle
potentially hostile environments.

# Overview

The cluster system relies on a single elected Leader in order to make
modifications.  Each client modification request sends the log entry to the
leader only, the leader then adds a log entry to itself, then replicates the
entry to all connected follower nodes.  When a Quorum has been reached (> 50%
of nodes have acknowledged the entry), the response is sent to the client.

Every member of the cluster is always attached to every other member of the
cluster (full mesh topology) and each node will be required to handle heartbeat
messages.  TCP/IP (with TLS) will be used as transport, this transport ensures
that messages are always delivered in order without gaps which reduces some
complexity as opposed to RAFT.

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


# Plug-in Callback System

The plugin system is used to interpret the Log messages that get replicated
across the cluster.  This may best be described using an example.

A common implementation of a plugin might consist of common actions such as:

   ReadKey, InsertKey, InsertOrReplaceKey, CompareAndSetKey, IncrementKey,
   DecrementKey.

Some of the above list of actions may have validators associated with them
which can fail (For instance, CompareAndSet may fail if the required value
doesn't match the current value).  If the validation by the plugin fails, the
entire request is rejected and no replication attempt is made.

One action might seem unnecessary, ReadKey, but if you need to guarantee you
are reading fresh data, it may be necessary to route this to the Leader rather
than reading potentially very stale cache from the current node.  Some plugins
may still choose to have a stale read capability, but should at least do some
due diligence on the cluster state (such as is the node still actively
connected to a leader).

The plugin may also choose to rewrite the log entry. For instance, the other
nodes have no need to perform an action on ReadKey, so a NoOp log entry might be
output instead.  A NoOp entry is still replicated to provide consistency
guarantees (which may be invalidated if the current Leader isn't really the
Leader any more). Similarly, log entires like InsertKey, CompareAndSetKey,
IncrementKey, and DecrementKey might all be rewritten to a simplified
InsertOrReplaceKey as an optimization to prevent the Follower nodes from needing
to do additional work.

If the validation step of the plugin fails for Follower receiving the replicated
log, the node will immediately go into an Error state and notify all connected
peers. This most likely means the node is corrupt.

In addition to the above log insertion/interpretation, the plug-in system must
also support serialization and deserialization of the data (regardless if
stored on disk or in memory) for bringing new nodes online.  For large datasets
it is strongly recommended to implement chunking of the data.

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


# System Configuration

- [R] ClusterName      - Name of the cluster to use when joining other nodes
                         (failure if mismatch)
- [R] SharedSecret     - Shared Secret for cluster authentication.  Will be
                         authenticated using HMAC using a peer-provided nonce to
                         prevent replay attacks
- [R] ServerList       - List of servers (with port) to attempt to connect to as
                         part of the cluster (does not have to be complete, but
                         should be).  Must all be from the same address class
                         (ipv4 vs ipv6)
- [R] Flags            - VOTE_ONLY (can only vote, does not actually receive
                         logs, essentially arbitrator mode), TLS_NOVERIFY_PEER
                         (don't verify the peer certificate)
- [R] MaximumRTT       - Maximum RTT response time in ms from any node before
                         evicting the node in an error state. This should be
                         at least 10x the average round trip time to handle
                         any packet rerouting due to interface issues.
- [R] MaximumLogSize   - Maximum size in bytes of the recorded log payloads.
                         Will purge the oldest entries until it reaches the
                         desired cumulative size.  This affects syncing after a
                         detached member rejoins the cluster, if older logs
                         have been purged, will require a full data
                         serialization and transfer.
- [R] Port             - Port to listen on
- [O] NodeIPAddress    - IP Address of local machine to use to connect to peers.
                         Must be of the same address class as the Server List.
                         If not provided, will choose a non-localhost and
                         non-link-local ip address at random.

# Node State

- State         - enum:
  - INIT:     Initializing - may not be connected to any nodes, this is the
              initial state.  When tracking servers, this state does not count
              toward quorum (and obviously won't receive logs).
  - JOIN:     Node is trying to join the cluster.  When tracking servers, this
              state does not count toward quorum and does not receive logs.
  - FOLLOWER: Not the primary node, just receiving logs from leader to apply and
              can participate in voting.
  - LEADER:   Primary Node.  Responsible for processing all requests.
  - VOTER:    Does not receive logs, only participates in voting (arbitration
              mode)
  - ERROR:    Node is in an error state (e.g. maximum latency, non-leader
              sending log entries), evicted from cluster.  Still counts toward
              quorum as it is expected to reconnect and re-sync.
- currentTerm   - u64  - Current node term, 0 if not set.  This may be
                         greater than the newest Log entry's term during
                         elections.
- LogID         - u64  - Last committed Log ID.  This is only needed during
                         joining where serialized data may have been received,
                         but not the log entries, so this is necessary to know
                         when to skip applying logs.
- Log[]         - list - Committed Log Entries containing Term, LogID, and
                         Payload.
- AvgLatencyMs  - u64  - The average latency in ms for the cluster
- ClusterID     - u64  - Unique cluster state id when a new cluster is brought
                         online.  This prevents 2 detached clusters using the
                         same nodes and configuration from trying to merge.
- Nodes[]       - list - Linked List of tracked node objects containing the
                         peer node ip address, port, state, statistics and so
                         on.
- VotedFor      - ptr   - Points to the node voted in the current term if any
- ElectionTimer - timer - Timer to try start a new election.  Reset if node
                          receives a RequestVote


## Nodes List

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
- Latency[]      - u64[] - Circular Array of last 4096 response latencies in ms
- Term           - u64   - Leader Only. Last known term of node
- LogID          - u64   - Leader Only. Last known LogID of node
- ConnectedPeers - u16   - Known number of connected peers that are in a leader,
                           follower, or voter state.
- HeartbeatTimer - timer - Timer to send heartbeat messages which fire off every
                           4x AvgLatencyMs (or 20ms, whichever is greater) in
                           node state Leader, Follower, Join, or Voter.
- Nonce          - bin32 - Nonce value sent to peer, used during authentication
                           with remote


## Calculated Node Variables

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

# Protocol Requests

## Base Message Format

Requests and responses share the same basic message format.  Numeric values are
sent in Big Endian (Network Byte Order):
`[MagicValue 4B][Version 1B][Sequence 8B][RequestType 2B][Code 2B][Length 4B][PayLoad ...length]`
- MagicValue: `MCLU`
- Version: `0x1`
- Sequence: Incremental sequence number for each peer to peer request to match
  requests to responses.
- RequestType: 16bit Big Endian integer representing the Request Type or
  Response Type as defined in the Request Types section.
- Code: Always 0 on requests, otherwise a Response Code
- Length: 32bit Big Endian integer representing the length of the payload (not
  inclusive of the length itself or any preceding fields)
- Payload: Custom payload per request type

## Response Codes

Possible response codes:
- `OK` (0x00): Good Response
- `MORE_DATA` (0x01): Repeat last request, more data is available.
- `BAD_REQUEST` (0x02): Could not parse request, malformed.
- `UNKNOWN_CLUSTER` (0x03): Cluster name is not recognized
- `BAD_NODE_ID` (0x04): Self-identified node ID of peer does not match
  connection address.
- `AUTH_FAILED` (0x05): HMAC verification failed
- `NOT_LEADER` (0x06): A message that can only be directed to a leader was sent.
- `ONLY_FROM_LEADER`: (0x07): A message was received that can only come from the
  leader but you are not the leader.
- `INSUFFICIENT_LOGS` (0x08): Insufficient logs contained on node to sync, must
  perform full sync.
- `OUT_OF_SYNC` (0x09): Node is out of sync, impossible Term/Log.  Must perform
  full sync.
- `TOO_OLD` (0x0A): Returned for RequestVote stating the reason the node isn't
  being voted for is it is out of date.
- `ALREAD_VOTED` (0x0B): Returned for RequestVote stating this node has already
  voted for another node.
- `CANT_APPLY` (0x0C): Log cannot be applied due to validation failure.  This
  is a critical Follower error.

## Latency Tracking

All Request Types sent to a remote node will insert a latency entry upon
the received response.

## Request Types

### AuthNonce

When a remote peer connects to the current node, BOTH nodes will immediately
send out an AuthNonce packet to the remote node to start mutual authentication.

RequestType: `0x01`
Response: `0x81`

#### Request Format
`[Len 1B][ClusterName][Len 1B][NodeId][Len 1B][Nonce]`
- ClusterName: System-wide configuration name of cluster
- NodeID: What node identifies itself as.  Either an IPv4 address or IPv6 with
  port in string form. E.g. `192.168.1.1:5555` or `[2620:2A::35]:5555`
- Nonce: Random 32byte (256bit) value used for HMAC authentication

#### Response

Contains no payload.  Can return one of these codes:
- `OK`
- `BAD_REQUEST`
- `UNKNOWN_CLUSTER`
- `BAD_NODE_ID`

##### Requestor Validations/Procedure
- If receive a code other than `OK`, disconnect. If known node, set to `INIT`
  state otherwise remove.
- If `OK`, done with flow.

##### Receiver Validations/Procedure
- If the `ClusterName` sent in the payload does not match, return
  `UNKNOWN_CLUSTER`
- If the `NodeID` in the Payload does not match the source ip address, return
  `BAD_NODE_ID`
- Otherwise , move to `Authenticate`

### Authenticate

RequestType: `0x02`
Response: `0x82`

After receiving a Nonce from the remote, the next message must be a follow-up
containing the actual authentication packet.

#### Request Format
`[HMAC Auth]`
- HMAC Auth: The HMAC-SHA256 result when using the received `Nonce` as the data
  and the System Configured `SharedSecret` as the key.

#### Response
`[ClusterID 8B][AvgLatencyMs 8B][Len 1B][LeaderAddress]`
- ClusterID - Unique 64bit (Big Endian) cluster id that is randomly generated
  when the cluster is first initialized to ensure any nodes attempting to
  rejoin that may have been detached are joining the same cluster.  0 if isn't
  in `LEADER`, `FOLLOWER`, or `VOTER` state.
- AvgLatencyMs - As known by peer at this point.  Even though it may not be
  the leader, it provides a hint so that the Joiner can start participating
  in Heartbeats.
- LeaderAddress - String representing leader ip address, same form as NodeID.
  Omitted if peer isn't in `LEADER`, `FOLLOWER`, or `VOTER` state.

Can return one of these codes:
- `OK`
- `BAD_REQUEST`
- `AUTH_FAILED`

##### Requestor Validations/Procedure
- All return values other than `OK` must result in an immediate disconnect.
- If `ClusterID` was returned, and we already know the `ClusterID` and it
  doesn't match:
  - If in `INIT` state, clear Node State Variables:
    - currentTerm
    - Log[]
    - ClusterID
    - Plug-in Data
  - If in `LEADER`, `FOLLOWER`, or `VOTER` state, ignore.
- If current node state is `INIT`, transition to `JOIN` state.

##### Respondor Validations/Procedure
- Validate HMAC, if invalid, return `AUTH_FAILED` and disconnect node.  If the
  node is a configured node, move to `INIT` state otherwise delete node entry.
- If Valid, return `OK`, and if ClusterID and LeaderAddress are known, send.


### Join

Join or re-join the cluster. Sent only to Leader node.

RequestType: `0x03`
Response: `0x83`

#### Request Format
`[LogTerm 8B][LogID 8B][Type 1B]`
- LogTerm: 64bit Last committed term that exists on this node, or 0 if none.
  This is NOT the Node's currentTerm counter which may be different.
- LogID: 64bit Last committed log id that exists on this node, or 0 if none
- Type:
   - `MEMBER` (0x01): Full cluster member
   - `VOTER` (0x02): Voter member only

#### Response Format
`[LogTerm 8B][LogID 8B][AvgLatencyMs 8B][Len 4B][PayLoad][Len 4B][NodeList]`
- LogTerm: Last committed Term ID
- LogID: Last committed Log ID
- AvgLatencyMs: The current average latency honored by nodes, used to calculate
  heartbeat timers, fault timers, and election timers.
- PayLoad: Optional. Serialized plug-in data to inject into node.  Only sent
  if requestor had passed a Term and Log ID of 0 and the Return Code is `OK`
  or `MORE_DATA`.  Not sent for type `Voter`
- NodeList: Comma delimited list of nodes known to the server that are
  participating in quorum.  Only returned on the `OK` response packet.

Can return one of these codes:
- `OK`
- `MORE_DATA`
- `BAD_REQUEST`
- `INSUFFICIENT_LOGS`
- `OUT_OF_SYNC`
- `NOT_LEADER`

##### Requestor Validations/Procedure
- On return code of `BAD_REQUEST` or `INSUFFICIENT_LOGS`, set `Term` and `LogID`
  to zero and retry Join request.
- On `NOT_LEADER`, unset known leader, wait for notification of new leader, once
  notified, try again.
- On `BAD_REQUEST` disconnect
- On `MORE_DATA`, process payload data and retry request with zero Term and
  LogID
- On `OK`, process payload data (if any).  Record Term and LogID in Node State.
  Transition to `FOLLOWER` state.

##### Respondor Validations/Procedure
- On bad parse, disconnect
- If Term > currentTerm, return `OUT_OF_SYNC`
- If LogID < oldest log, return `INSUFFICIENT_LOGS`
- If Log[LogID].Term != Term, return `OUT_OF_SYNC`
- If LogTerm and LogID can be played from current log entries, this is `OK`
- If LogTerm and LogID are 0, then serialize data and send (possibly chunked),
  use `MORE_DATA` return code if chunked, except on last block which will return
  `OK`.
- If returning `OK`, send AppendEntries with AddNode to the other nodes and wait
  for quorum before sending response to client (if we don't get quorum then we
  must no longer be the leader and will return `NOT_LEADER`).
- If LogTerm and LogID are non-zero, send the gap in the logs to the node via
  AppendEntries, otherwise send **all** logs and node will store those logs for
  future syncing.


### HeartBeat

A heartbeat packet is queued to be sent to every node at a rate of 4x
AvgLatency or 20ms, whichever is greater.  This does mean that 2 heartbeats will
occur during this interval as each node will initiate a heartbeat to the other
in order to measure latency. Any node in the state of `Leader`, `Follower`,
`Voter`, or `Join` must participate in Heartbeats.

RequestType: `0x04`
Response: `0x84`

#### Request Format
There is no payload for the request

#### Response Format
`[knownPeerCnt 4B][joinedPeerCnt 4B][activePeerCnt 4B][state 2B]`
- knownPeerCnt: Total number of known nodes that could participate in the
  cluster.
- joinedPeerCnt: Total number of nodes that are supposed to be participating
  in the cluster for quorum (but may not be due to faults).  A node that has
  gracefully disconnected is in the knownPeerCnt but not in the joinedPeerCnt.
- activePeerCnt: Number of nodes actively responding (not in an error or
  disconnected state)
- state: advertised state from the node

Can return one of these codes:
- `OK`

##### Requestor Validations/Procedure
- Maintain heartbeat timer to enqueue another heartbeat.  Do not send more than
  1 heartbeat at a time, wait to receive a heartbeat before starting the timer
  to send the next.

##### Responder Validations/Procedure
- Collect metadata and respond, there are no necessary validations


### RequestVote

Request vote for oneself to become the leader.  Only called upon an Election
Timer timeout.  Requesting node may be in the Leader, Follower, or Join state.
The only time the Join state will call this request is when there are no nodes
in the Leader or Follower state, but there is a Quorum of connected nodes based
on the server configuration.

This request will be sent to all members of the cluster that are in `Leader` or
`Follower` state (or if *all* members are in `Join` it will be sent to those
as well).  Record self as `VotedFor` in state before sending and increment
the internally tracked current Term.

RequestType: `0x05`
Response: `0x85`

#### Request Format
`[currentTerm 8B][LogTerm 8B][LogID 8B]`
- currentTerm: Current term requesting votes for
- LogTerm: Last Committed term number as known to node
- LogID: Current LogID as known to node

#### Response Format
No Payload. Can return one of these codes:
- `OK`
- `TOO_OLD`
- `ALREADY_VOTED`


##### Requestor Validations/Procedure
- On `OK`, increment the positive vote counter, others increment negative
  vote counter.
- If the positive vote counter reaches quorum, transition to `LEADER`, append a
  NoOp log entry with the current Term which will notify all other nodes
  of the new leadership.
- If the negative vote counter reaches quorum, wait for next election or
  notification of another node winning.

##### Responder Validations/Procedure
- if `LogTerm` is less than our recorded log term, or `LogID` is less than
  our recorded log id, return `TOO_OLD`
- if packet currentTerm is less than our Node's current term, return
  `ALREADY_VOTED`
- Otherwise set our currentTerm to the one received and return `OK`, but do
  NOT set a new leader, wait for an `AppendEntries` with >= currentTerm to
  set the new leader.


### Finish

This request will be sent from a node performing a graceful shutdown to the
Leader to detach from the cluster.  The effect of detaching from the cluster
will reduce the total number of nodes participating in Quorum.

RequestType: `0x06`
Response: `0x86`

#### Request Format
There is no request format.

#### Response Format
There is no response format.
Can return one of these codes:
- `OK`
- `NOT_LEADER`

##### Requestor Validations/Procedure
- Continue replying to all messages until an `OK` is returned, do not change
  own state.
- If receive `NOT_LEADER`, wait until a new leader is advertised, and retry.
- If receive `OK`, terminate all connections to peers and clean up

##### Responder Validations/Procedure
- If not leader, return `NOT_LEADER`
- Send `AppendEntries` with `RemoveNode` to all `Followers`, wait on Quorum,
  then return `OK`.  If `Quorum` not acheived, return `NOT_LEADER`


### AppendEntries

Append a log entry to all follower nodes.  Only performed by the Leader.

RequestType: `0x07`
Response: `0x87`

#### Request Format
`[Cnt 4B]...[LogTerm 8B][LogID 8B][Type 2B][Len 4B][Data]...` (repeats for each entry)
- Cnt: Number of log entries to apply
- LogTerm: Term of log entry
- LogID: ID of log entry
- Type: `Log` (0x01), `AddNode` (0x02), `RemoveNode` (0x03), `NewTerm` (0x04)
- Data:
  - `Log`: payload data for plugin (possibly empty indicating NoOp)
  - `AddNode`:  NodeID format of node being added
  - `RemoveNode`: NodeID format of node being removed
  - `NewTerm`: AvgLatencyMs as 64bit Integer

#### Response Format

No response. Can return one of these codes:
- `OK`
- `ONLY_FROM_LEADER`
- `CANT_APPLY`

##### Requestor Validations/Procedure
- Triggered based on a new node coming online, an old node gracefully
  terminating, a new Term, or a new client request to be replicated.

##### Responder Validations/Procedure
- If `Type` is not `NewTerm` and request didn't come from the current
  Leader, return `ONLY_FROM_LEADER`.
- If `Type` is `NewTerm` and `LogTerm` is less than `currentTerm`, or `LogID`
  less than the last known log id return `CANT_APPLY` and disconnect from
  cluster and reconnect.
- Process each log entry sequentially, they are in increasing order if there
  are more than one, and update internal log and term counters.  If the
  plugin callback fails, return `CANT_APPLY`, then disconnect from the cluster
  as this is a critical failure, and reconnect.


### ClientRequest

This is a request from a client to the Leader node.  The request will come over
a channel from a Follower, or directly initiated if the requesting node is the
Leader.  Client requests will not receive a successful response until quorum is
reached on the AppendEntries relayed from the Leader to the followers, or an
error has occurred.

RequestType: `0x0A`
Response: `0x8A`

#### Request Format

`[Len 4B][Data]`
- Data: The payload for the plugin to process

#### Response Format
`[LogTerm 8B][LogID 8B][Len 4B][Data]`
- LogTerm: Term this record was committed in
- LogID: The ID of the record
- Data: any custom response data from the plugin



