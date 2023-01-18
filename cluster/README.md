# Cluster Implementation

Inspired by the RAFT Consensus algorithm by Diego Ongaro:
[Consensus: Bridging Theory and Practice](https://web.stanford.edu/~ouster/cgi-bin/papers/OngaroPhD.pdf)

Also took into consideration modification recommendations by Henrik Ingo:
[Four modifications for the Raft consensus algorithm](https://www.openlife.cc/sites/default/files/php_uploads/4-modifications-for-Raft-consensus.pdf)

Though this implementation doesn't strictly conform to the above, it attempts
to in spirit along with some real world modifications to better handle
potentially hostile environments.

A few modifications include:
  1. All nodes send heartbeats to eachother that are tracked independently and
     are not based on current traffic loads.  These also do not generate NoOp
     Log entries like RAFT.
  2. Timers are self-adjusting to current network conditions which can better
     handle route changes across geographic areas.
  3. Authentication and Transport are strictly defined.
  4. Cluster Initialization and Cluster Verification are defined.
  5. Define learning of new cluster members as well as graceful shutdown of
     cluster members, including leaders.


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
across the cluster.  It is up to the plugin to define the function and form
of each log message, and also to apply it to its backend with whatever
representation that may be.

This may best be described using an example.

A common implementation of a plugin might consist of common actions such as:

   ReadKey, InsertKey, InsertOrReplaceKey, CompareAndSetKey, IncrementKey,
   DecrementKey, and NoOp.

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
to do additional work that the Leader has already performed.

If the validation step of the plugin fails for Follower receiving the replicated
log, the Follower node will immediately go into an Error state and notify all
connected peers. This most likely means the node is corrupt and will need to
reinitialize its state from scratch.

In addition to the above log insertion/interpretation, the plug-in system must
also support serialization and deserialization of the data (regardless if
stored on disk or in memory) for bringing new nodes online.  Only so many log
entries may be maintained for incremental state restoration, so it may be
necessary to do a bulk sync (especially if we're talking multiple-GB of data).
For large datasets it is strongly recommended to implement chunking of the data
within the plugin dataset.

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
- [O] MaximumRTT       - Maximum RTT response time in ms from any node before
                         evicting the node in an error state. This should be
                         at least 10x the average round trip time to handle
                         any packet rerouting due to interface issues. Defaults
                         to 3000ms (3s).
- [O] MaximumLogSize   - Maximum size in bytes of the recorded log payloads.
                         Will purge the oldest entries until it reaches the
                         desired cumulative size.  This affects syncing after a
                         detached member rejoins the cluster, if older logs
                         have been purged, will require a full data
                         serialization and transfer.  Logs are currently kept
                         in memory only.  Defaults to 10,000,000 (10MB).
- [O] Port             - TCP Port to listen on. Defaults to 7150.
- [O] NodeIPAddress    - IP Address of local machine to use to connect to peers.
                         Must be of the same address class as the Server List.
                         If not provided, will choose a non-localhost and
                         non-link-local ip address at random.

# Node States

Enumeration of possible node states.

- INIT:     Initializing - Not be connected to any nodes, and not attempting
            to connect. This is the initial state.  Counts towards quorum if
            isError. Next state is CONN or JOIN (depending on initiator).
- CONN:     Attempting to connect at the OS level, but not yet established.
            Counts towards quorum if isError.  Next state is JOIN.  Failure
            state is INIT.
- AUTH1:    Sent Authenticate, but have not received equivalent from remote.
            Next state is AUTH2 once authentication request from remote has
            been received.
- AUTH2:    Received Authenticate, but have not yet received response from our
            own authentication request. Next state is JOIN once authentication
            response is received.
- JOIN:     Node is trying to join the cluster.  Counts towards quorum if
            isError.  Next state is FOLLOWER or VOTER.
- FOLLOWER: Not the primary node, just receiving logs from leader to apply and
            can participate in voting. Can transition to LEADER on election.
- LEADER:   Primary Node.  Responsible for processing all requests.
- VOTER:    Does not receive logs, only participates in voting (arbitration
            mode).
- FINISH:   Node requested graceful disconnect.  Does not count toward quorum.
            Differentiated from INIT in the fact that we expect the node to
            initiate the connection to us and we will never initiate the
            connection.  Next state is JOIN.

# Node Variables

- State         - enum - One of the Node States
- isError       - bool - Only applicable to INIT, CONN, and JOIN states.  If the
                         node was evicted or unexpectedly disconnected, this
                         flag is set to true until it successfully rejoins the
                         cluster.
- currentTerm   - u64  - Current node term, 0 if not set.  This may be
                         greater than the newest Log entry's term during
                         elections.
- LogID         - u64  - Last committed Log ID.  This is only needed during
                         joining where serialized data may have been received,
                         but not the log entries, so this is necessary to know
                         when to skip applying logs.
- Log[]         - list - Committed Log Entries containing Term, LogID, and
                         Payload.  Stored only in memory currently.
- LatencyMs     - u16  - The currrent max latency in ms for the cluster.
                         Learned from leader node, if no leader, calculated
                         based on connected nodes.
- ClusterID     - u64  - Unique cluster state id when a new cluster is brought
                         online.  This prevents 2 detached clusters using the
                         same nodes and configuration from trying to merge.
- Nodes[]       - list - Linked List of tracked node objects containing the
                         peer node ip address, port, state, statistics and so
                         on.
- VotedFor      - ptr   - Points to the node voted in the current term if any
- ElectionTimer - timer - Timer to try start a new election.  Reset if node
                          receives a RequestVote or a HeartBeat from the current
                          Leader.
- Flags         - bits  - Flags about the connection, for instance we need
                          `AUTHENTICATED_PEER`, and `AUTHENTICATED_SELF` to make
                          sure a node is allowed to transition to join.
- NodeConnTimer - timer - Timer that triggers on a random interval (1-3s) to
                          scan connections of known nodes to see if new
                          connections need to be spawned.

## Nodes List

The list of servers maintained in the node state contains these data elements:

- Ip Address     - text  - Ip address of peer
- Port           - u16   - Peer Port
- State          - enum  - One of the Node States.
- ConnectTimer   - et    - Elapsed Time since connect. Monotonic tracking of how
                           long this node has been connected.
- LastMsgTimer   - et    - Elapsed Time since last message. Monotonic tracking
                           of last message time to detect lagged nodes and
                           heartbeats.
- LatencyTotal   - u64   - Sum of all latencies recorded up to the maximum of
                           4096.  If LatencyCnt == 4096, will subtract
                           (LatencyTotal / 4096) before adding the latest
                           recorded latency.  Otherwise we'd need to maintain
                           an array.  This method should also 'soften' the
                           impact of temporary spikes in latency.  In the
                           future, it may make sense to dynamically set the
                           LatencyCntMax to some period which average latency
                           covers some predefined time period like 60s.
- LatencyCnt     - u16   - Count of latencies recorded up to 4096.
- Term           - u64   - Leader Only. Last known term of node
- LogID          - u64   - Leader Only. Last known LogID of node
- ConnectedPeers - u16   - Known number of connected peers that are in a leader,
                           follower, or voter state.
- HeartbeatTimer - timer - Timer to send heartbeat messages which fire off every
                           4x LatencyMs (or 20ms, whichever is greater) in
                           node state Leader, Follower, Join, or Voter.
- Nonce          - bin32 - Nonce value sent to peer, used during authentication
                           with remote


## Calculated Node Variables

- NodeLatencyMs -     LatencyTotal / LatencyCnt
- LatencyMs -         Received during Vote for leader from proposed leader.  If
                      proposing, perform MAX(Servers[n].Latency) and use
                      that value (minimum 1, max 65535).
- ElectionTimeoutMs - Base value is 10 x LatencyMs with a minimum value of
                      100ms.  Nodes will use a random value of 1.0x to 2x the
                      base value.  Every reset of the timer will calculate a
                      new value to ensure random distribution.
- FaultTimeoutMs -    25x LatencyMs or Configured Maximum RTT, whichever is
                      less.  If a reply to a message takes longer than this, the
                      node is put into an Error state and disconnected.

# Protocol Requests

## Base Message Format

Requests and responses share the same basic message format.  Numeric values are
sent in Big Endian (Network Byte Order):
`[MagicValue 4B][Version 1B][ReqResp 1B][Sequence 8B][Length 4B][Tag 2AN][TagType 1B][TagLength 4B][TagData][...]`
- MagicValue: `MCLU`
- Version: `0x01`
- ReqResp: `0x00` for requests, `0x01` for responses.
- Sequence: Incremental sequence number for each peer to peer request to match
  requests to responses.
- Length: 32bit Big Endian integer representing the length of the payload (not
  inclusive of the length itself or any preceding fields)
- Tag: 2byte alpha-numeric tag name for data specific to request.  There is no
  defined limit for the number of tags (other than the number of combinations of
  alpha-numeric values).
- TagType: 1 byte tag type. 1=Text, 2=Int8, 3=Int16, 4=Int32, 5=Int64, 6=Binary
- TagLength: 4 byte tag data length (32bit big endian integer)
- TagData: Data associated with tag.

## Latency Tracking

All Request Types sent to a remote node will insert a latency entry upon
the received response.


## Message Tags

### AU - HmacAuthentication - B
HMAC Auth: The HMAC-SHA256 result when using the received `Nonce` as the data
and the System Configured `SharedSecret` as the key.

### CA - CountActivePeers - Int16
Number of nodes actively responding (not in an error or disconnected state)

### CI - Cluster ID - Int64
Unique 64bit (Big Endian) cluster id that is randomly generated when the cluster
is first initialized to ensure any nodes attempting to rejoin that may have been
detached are joining the same cluster.

### CJ - CountJoinedPeers - Int16
Total number of nodes that are supposed to be participating in the cluster for
quorum (but may not be due to faults).  A node that has gracefully disconnected
is in the CountKnownPeers but not CountJoinedPeers.

### CN - ClusterName - AN
Name of the cluster.  Used as a sanity check to ensure the cluster being
connected to matches the current known cluster.

### CP - CountKnownPeers - Int16
Total number of known nodes that could participate in the cluster.

### CT - CurrentTerm - Int64
Current term.  Used when requesting votes to elect a new leader, the current
term will be incremented and votes will be collected.  This is always greater
than the known latest LogTerm on all nodes in the cluster (assuming all nodes
are in sync).

### LA - LeaderAddress - IPv4/IPv6 Address plus Port
String representing leader ip address, same form as NodeID.  Only peers that are
in the `LEADER`, `FOLLOWER`, or `VOTER` state may respond with this.

### LI - LogID - Int64
Last committed log id that exists on the node.

### LM - LatencyMs - Int16
Cluster Latency in milliseconds as known by peer.  If a Leader or Follower in
an existing cluster, this is the cluster-wide known value as determined by the
current leader.  Otherwise this is a best effort guess by the peer based on
other connections.  This is needed for a newly connected node to participate in
heartbeats.

This value is also used to calculate fault and election timers.

### LT - LogTerm - Int64
Last committed term that exists on this node. This is **not** the Node's
currentTerm counter which may be different.

### NI - Node ID - IPv4/IPv6 Address plus Port
What node identifies itself as.  Either an IPv4 address or IPv6 with port in
string form. E.g. `192.168.1.1:5555` or `[2620:2A::35]:5555`

### NL - NodeList - List of IPv4/IPv6 Address plus Port member list
Comma delimited list of nodes known to the server that are articipating in
quorum. Each node is in NodeID format.

### NO - Nonce - B
Random 32byte (256bit) value used for HMAC authentication

### NT - NodeType - Int8
Configured Node Type

Possible Values:
 - 0x01: Member - full cluster member
 - 0x02: Voter - voter-only (quorum participant, but does not receive logs)

### RC - Response Code - Int16
Required on all responses.

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
- `ALREADY_VOTED` (0x0B): Returned for RequestVote stating this node has already
  voted for another node.
- `CANT_APPLY` (0x0C): Log cannot be applied due to validation failure.  This
  is a critical Follower error.

### RT - Request Type - Int16
Required on **all** requests *and* responses.

Specific request types:
- 0x0001: Authenticate
- 0x0002: Heartbeat
- 0x0003: Join
- 0x0004: RequestVote
- 0x0005: Finish
- 0x0006: AppendEntries
- 0x0007: SyncPluginData
- 0x0100: ClientRequest

### SP - SerializedPluginData - Binary
Plugin-specific serialized plugin data.  May be complete serialized data or
partial depending on implementation.  If partial, additional follow-up requests
will be made to complete sync.

### SR - SerializedPluginResponseData - Binary
Plugin-specific serialized plugin response data.  Only ever sent back in
response to a ClientRequest.

### ST - NodeState - Int8
State of the node:
 - 0x01: INIT
 - 0x02: CONN
 - 0x03: AUTH1
 - 0x04: AUTH2
 - 0x05: JOIN
 - 0x06: FOLLOWER
 - 0x07: LEADER
 - 0x08: VOTER
 - 0x09: FINISH

## Request Type Descriptions

### Authenticate

When a remote peer connects to the current node, BOTH nodes will immediately
send out an Authenticate packet to the remote node to start mutual authentication.
The request will identify the current node to the peer and provide a Nonce for
finalizing authentication.

Required Request Tags: `RT` (Request Type), `CN` (Cluster Name), `NI` (Node ID),
  `NO` (Nonce)

Required Response Tags: `RT` (RequestType), `RC` (ResponseCode),
  `AU` (HmacAuthentication)

Optional Response Tags: `CI` (ClusterID), `LA` (LeaderAddress), `LM` (LatencyMs)

Can return one of these codes:
- `OK`
- `BAD_REQUEST`
- `UNKNOWN_CLUSTER`
- `BAD_NODE_ID`

#### Requestor Validations/Procedure
- If receive a code other than `OK`, disconnect. Set self to `INIT`
  state otherwise remove.
- If `OK`, validate HMAC.  On failure to validate HMAC, disconnect, set to `INIT`

#### Receiver Validations/Procedure
- If node state not AUTH1, disconnect.
- Transition state to AUTH2
- If the Cluster Name sent in the payload does not match, return
  `UNKNOWN_CLUSTER` and disconnect
- If the Node ID in the Payload does not match the source ip address, return
  `BAD_NODE_ID` and disconnect
- Use Nonce to generate authentication hmac


### Join

Join or re-join the cluster. Sent only to Leader node.

Required Request Tags: `RT` (Request Type), `NT` (Node Type)

Optional Request Tags: `LT` (Log Term), `LI` (Log ID)

Required Response Tags: `RT` (Request Type), `RC` (Response Code),
  `LT` (LogTerm), `LI` (LogID), `LM` (LatencyMs), `NL` (NodeList)

Can return one of these codes:
- `OK`
- `MORE_DATA`
- `BAD_REQUEST`
- `INSUFFICIENT_LOGS`
- `OUT_OF_SYNC`
- `NOT_LEADER`

#### Requestor Validations/Procedure
- On return code of `BAD_REQUEST` or `INSUFFICIENT_LOGS`, set `LogTerm` and
  `LogID` to zero and retry Join request.
- On `NOT_LEADER`, unset known leader, wait for notification of new leader, once
  notified, try again.
- On `BAD_REQUEST` disconnect
- On `MORE_DATA`, process payload data and retry request with zero LogTerm and
  LogID
- On `OK`, process payload data (if any).  Record LogTerm and LogID in Node State.
  Transition to `FOLLOWER` state.

#### Respondor Validations/Procedure
- On bad parse, disconnect
- If LogTerm > currentTerm, return `OUT_OF_SYNC`
- If LogID < oldest log, return `INSUFFICIENT_LOGS`
- If Log[LogID].Term != Term, return `OUT_OF_SYNC`
- If LogTerm and LogID can be played from current log entries, this is `OK`
- If LogTerm and LogID are not specified, this is also OK, the node will request
  SyncPluginData.
- If returning `OK`, send AppendEntries with AddNode to the other nodes and wait
  for quorum before sending response to client (if we don't get quorum then we
  must no longer be the leader and will return `NOT_LEADER`).
- If LogTerm and LogID are non-zero, send the gap in the logs to the node via
  AppendEntries, otherwise send **all** logs and node will store those logs for
  future syncing (not used for applying as SerializedPluginData did this).


### SyncPluginData
SerializedPluginData is only sent in the response. This data is used to bring
the node into sync up to the LogTerm and LogID at the time the sync was started.
This may be chunked, meaning the client must re-send the same request until
the result is no longer `MORE_DATA`.  After all plugin data is synced, the
server will send AppendEntries for any data processed

Required Request Tags: `RT` (RequestType)

Required Response Tags: `RT` (Requesttype), `RC` (ResponseCode), `LT` (LogTerm),
   `LI` (LogId), `SP` (SerializedPluginData)

Can return one of these codes:
- `OK`
- `MORE_DATA`
- `BAD_REQUEST`
- `NOT_LEADER`

### HeartBeat

A heartbeat packet is queued to be sent to every node at a rate of 4x
LatencyMs or 20ms, whichever is greater.  This does mean that 2 heartbeats will
occur during this interval as each node will initiate a heartbeat to the other
in order to measure latency. Any node in the state of `Leader`, `Follower`,
`Voter`, or `Join` must participate in Heartbeats.

Required Request Tags: `RT` (RequestType)

Required Response Tags: `RT` (RequestType),`RC` (ResponseCode),
  `CP` (CountKnownPeers), `CJ` (CountJoinedPeers), `CA` (CountActivePeers),
  `ST` (NodeState)

Can return one of these codes:
- `OK`

#### Requestor Validations/Procedure
- Maintain heartbeat timer to enqueue another heartbeat.  Do not send more than
  1 heartbeat at a time, wait to receive a heartbeat before starting the timer
  to send the next.

#### Responder Validations/Procedure
- If heartbeat request originated from current leader, reset election timer.
- Collect metadata and respond.


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

Required Request Tags: `RT` (RequestType), `CT` (CurrentTerm), `LT` (LogTerm),
  `LI` (LogID)

Required Response Tags: `RT` (RequestType), `RC` (ResponseCode)

Can return one of these codes:
- `OK`
- `TOO_OLD`
- `ALREADY_VOTED`


#### Requestor Validations/Procedure
- On `OK`, increment the positive vote counter, others increment negative
  vote counter.
- If the positive vote counter reaches quorum, transition to `LEADER`, append a
  NoOp log entry with the CurrentTerm as the LogTerm which will notify all other
  nodes of the new leadership.
- If the negative vote counter reaches quorum, wait for next election or
  notification of another node winning.

#### Responder Validations/Procedure
- if `LogTerm` is less than our recorded log term, or `LogID` is less than
  our recorded log id, or the received `CurrentTerm` is less than or equal
  to our `LogTerm` return `TOO_OLD`
- if packet currentTerm is less than our Node's currentTerm, return
  `ALREADY_VOTED`
- Otherwise set our currentTerm to the one received and return `OK`, but do
  NOT set a new leader, wait for an `AppendEntries` with >= currentTerm to
  set the new leader.

### Finish

This request will be sent from a node performing a graceful shutdown to the
Leader to detach from the cluster.  The effect of detaching from the cluster
will reduce the total number of nodes participating in Quorum.

Only nodes that are `Leader`, `Follower`, or `Voter` may

Required Request Tags: `RT` (RequestType)

Required Response Tags: `RT` (RequestType), `RC` (ResponseCode)

Can return one of these codes:
- `OK`
- `NOT_LEADER`

#### Requestor Validations/Procedure
- Continue replying to all messages until an `OK` is returned, do not change
  own state.
- If receive `NOT_LEADER`, wait until a new leader is advertised, and retry.
- If receive `OK`, terminate all connections to peers and clean up

#### Responder Validations/Procedure
- If not leader, return `NOT_LEADER`
- Send `AppendEntries` with `RemoveNode` to all `Followers` and `Voters`, wait
  on Quorum, then return `OK`.  If `Quorum` not acheived, return `NOT_LEADER`


### AppendEntries

Append a log entry to all follower nodes.  Only performed by the Leader.

Required Request Tags: `RT` (RequestType), `LT` (LogTerm), `LI` (LogId),
  `SP` (SerializedPluginData)

Required Response Tags: `RT` (RequestType), `RC` (ResponseCode)

Can return one of these codes:
- `OK`
- `ONLY_FROM_LEADER`
- `BAD_REQUEST`
- `CANT_APPLY`


#### Requestor Validations/Procedure
- Triggered based on a new node coming online, an old node gracefully
  terminating, a new Term, or a new client request to be replicated.

#### Responder Validations/Procedure
- If `LT` is equal to  `currentTerm`, and request didn't come from the current
  Leader, return `ONLY_FROM_LEADER`.
- If `LT` is less than `currentTerm`, log `CANT_APPLY` and disconnect from
  cluster and reconnect.
- If `LT` is greater than `currentTerm`, and `LI` is exactly 1 greater than
  last known `LogId` then set the new leader.  If `LI` is otherwise not valid,
  return `CANT_APPLY` and disconnect.
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

Required Request Tags: `RT` (RequestType), `SP` (SerializedPluginData)

Required Response Tags: `RT` (RequestType), `RC` (ResponseCode)

Optional Response Tags: `SR` (SerializedPluginResponeData)


# Flows

## Startup

- Read Configuration
- Create Node List from Configuration
- Listen on Configured port for inbound connections
- Start NodeConnection Timer with 0ms interval (immediate fire)

## Node Connection (Timer)

- Cycle across all nodes in the INIT state, transition those nodes to the
  CONN state and start connection attempts.
- Pick random time between 1-3s to trigger self again

## Outbound Connection Established

- Start authentication flow by sending Authenticate
- Transition node's state in node list to AUTH1

## Inbound Connection Established

- Lookup node in node list
  - If found and state in node list is NOT: [INIT, CONN, FINISH], then
    disconnect and stop
  - If not found, Add Node to node list
  - Set state to AUTH1
  - Start authentication flow by sending Authenticate

## Connection Failed

- If state in node list is NOT [INIT, CONN, FINISH, AUTH1, AUTH2, JOIN], then set isError to
  true on node in node list.
- If state in node list is NOT [FINISH], then set state to INIT.
- Cleanup any session related data.
- If isError in node list is false (and therefore not participating in quorum),
  verify node is listed in configured server list, if not, purge from known
  node list.

## Authentication Request Received
- If node state not AUTH1, disconnect.
- Transition state to AUTH2
- If the Cluster Name sent in the payload does not match, return
  `UNKNOWN_CLUSTER` and disconnect
- If the Node ID in the Payload does not match the source ip address, return
  `BAD_NODE_ID` and disconnect
- Use Nonce to generate authentication hmac and send response

## Authentication Response Received
- If node state is not AUTH2, disconnect
- If receive a code other than `OK`, disconnect.
- Validate HMAC.  On failure to validate HMAC, disconnect.
- If cluster id received and different than known cluster id, disconnect.
- If Leader Address received and don't have a current leader, record.
- If not currently joined to the cluster ourselves, but LatencyMs received, record.
- Transition to JOIN state



