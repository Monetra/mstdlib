

System Configuration
====================
- [R] Cluster Name     - Name of the cluster to use when joining other nodes (failure if mismatch)
- [R] SharedSecret     - Shared Secret for cluster authentication.  Will be authenticated using HMAC using a peer-provided nonce to prevent replay attacks
- [R] Server List      - List of servers (with port) to attempt to connect to as part of the cluster (does not have to be complete, but should be).  Must all be from the same address class (ipv4 vs ipv6)
- [R] Election Timeout - Timeout in milliseconds between elections.  This should be roughly 10x the max average latency between nodes (the maximum of the averages between each node pair).  Each node will choose a random value between this provided number and 2x this provided number for starting elections to prevent multiple nodes from starting an election simultaneously.
- [R] Flags            - VOTE_ONLY (can only vote, does not actually receive logs, essentially arbitrator mode), TLS_NOVERIFY_PEER (don't verify the peer certificate)
- [R] Port             - Port to listen on
- [O] Node IP Address  - IP Address of local machine to use to connect to peers.  Must be of the same address class as the Server List.  If not provided, will choose a non-localhost and non-link-local ip address at random.

Node State
==========

