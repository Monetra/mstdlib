# Reading Linux CPU quotas

CPU Quotas in Linux are handled via CGroups.  Unfortunately there is a v1 and a v2 way to read them.  The easiest way to recognize is to read `/proc/self/cgroup` and if there is exactly 1 line and it starts with `0::`, we are using cgroup v2.  Otherwise, it is cgroup v1. 

Any step that fails probably means the cpu isn't limited at all and should treat as unlimited.

## cgroup v2

### Locate CGroup path for CPU limits
Line from `/proc/self/cgroup` is `0::${cgroup_path}`

Read CPU limits from `/sys/fs/cgroup/${cgroup_path}/cpu.max`, format is `$MAX $PERIOD[ $BURST]`. The 3rd column `$BURST` is optional so it may or may not exist. 

NOTE: In cgroup v2, `/sys/fs/cgroup/` is a mandatory mount point so we don't have to scan the mounts like cgroup v1.

### Calculate the number of CPUs

`MAX` may be the literal word "max", if so, this means unlimited (no throttle).

Otherwise, to get the number of allowed CPUs, divide `MAX` by `PERIOD`.  Round the result to the nearest integer.  If rounds to zero use 1.


## cgroup v1

### Locate CGroup path for CPU limits

Format of `/proc/self/cgroup` contains multiple lines in the format of `${HIERARCHY_ID}:${CONTROLLER_LIST}:${CGROUP_PATH}`.  The `HIERARCHY_ID` can be ignored.  The `CONTROLLER_LIST` is a comma-delimited list of subsystems, the one we are interested in here is `cpu` (however we can't just substring it as there are `cpuacct` and `cpuset` subsystems, so split on comma and match the entry).

Example:
```
12:rdma:/
11:blkio:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
10:memory:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
9:cpu,cpuacct:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
8:perf_event:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
7:freezer:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
6:hugetlb:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
5:devices:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
4:cpuset:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
3:pids:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
2:net_cls,net_prio:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
1:name=systemd:/kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f
0::/system.slice/containerd.service
```

We can see in this example that `HIERARCHY_ID` #9 contains the entry we are looking for, so we save this `CGROUP_PATH`.


### Locate the Mount point for Cgroup information

With cgroup v1, the mount point for cgroup information can be anywhere as there is no defined standard path.  We must scan the existing mounts to locate the relevant entry.

In order to do that we need to read in `/proc/self/mountinfo`, the format is described in proc(5):
```
36 35 98:0 /mnt1 /mnt2 rw,noatime master:1 - ext3 /dev/root rw,errors=continue
 |  |  |    |     |     |          |          |      |      |
(1)(2)(3:4)(5)   (6)   (7)        (8)        (9)   (10)    (11)
```

We can use a regex like this to evaluate each line:
```
^(\d+) (\d+) (\d+):(\d+) (\S+) (\S+) (\S+) ((?:\S+:\S+ ?)*) - (\S+) (\S+) (\S+)$
```

In particular, we are scanning for #9 == `cgroup` and #11 contains `CONTROLLER_LIST` (it may have other entries like rw).  On a match, extract #6 as the base mount point.

Example:
```
2626 2312 0:290 / / rw,relatime master:766 - overlay overlay rw,lowerdir=/var/lib/containerd/io.containerd.snapshotter.v1.overlayfs/snapshots/183/fs,upperdir=/var/lib/containerd/io.containerd.snapshotter.v1.overlayfs/snapshots/660/fs,workdir=/var/lib/containerd/io.containerd.snapshotter.v1.overlayfs/snapshots/660/work
2627 2626 0:292 / /proc rw,nosuid,nodev,noexec,relatime - proc proc rw
2628 2626 0:293 / /dev rw,nosuid - tmpfs tmpfs rw,size=65536k,mode=755
2629 2628 0:294 / /dev/pts rw,nosuid,noexec,relatime - devpts devpts rw,gid=5,mode=620,ptmxmode=666
2630 2628 0:248 / /dev/mqueue rw,nosuid,nodev,noexec,relatime - mqueue mqueue rw
2631 2626 0:261 / /sys ro,nosuid,nodev,noexec,relatime - sysfs sysfs ro
2632 2631 0:295 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime - tmpfs tmpfs rw,mode=755
2633 2632 0:25 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/systemd ro,nosuid,nodev,noexec,relatime master:10 - cgroup cgroup rw,xattr,name=systemd
2634 2632 0:28 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/net_cls,net_prio ro,nosuid,nodev,noexec,relatime master:14 - cgroup cgroup rw,net_cls,net_prio
2635 2632 0:29 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/pids ro,nosuid,nodev,noexec,relatime master:15 - cgroup cgroup rw,pids
2636 2632 0:30 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/cpuset ro,nosuid,nodev,noexec,relatime master:16 - cgroup cgroup rw,cpuset
2637 2632 0:31 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/devices ro,nosuid,nodev,noexec,relatime master:17 - cgroup cgroup rw,devices
2638 2632 0:32 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/hugetlb ro,nosuid,nodev,noexec,relatime master:18 - cgroup cgroup rw,hugetlb
2639 2632 0:33 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/freezer ro,nosuid,nodev,noexec,relatime master:19 - cgroup cgroup rw,freezer
2640 2632 0:34 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/perf_event ro,nosuid,nodev,noexec,relatime master:20 - cgroup cgroup rw,perf_event
2641 2632 0:35 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/cpu,cpuacct ro,nosuid,nodev,noexec,relatime master:21 - cgroup cgroup rw,cpu,cpuacct
2642 2632 0:36 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/memory ro,nosuid,nodev,noexec,relatime master:22 - cgroup cgroup rw,memory
2643 2632 0:37 /kubepods/burstable/podb34634ea-cd65-4099-bee5-ad4de9264ffb/010e9324ada51cccb6d677cfbb55ede98876d294fd084e8498b6c1fc69056b1f /sys/fs/cgroup/blkio ro,nosuid,nodev,noexec,relatime master:23 - cgroup cgroup rw,blkio
2644 2632 0:38 / /sys/fs/cgroup/rdma ro,nosuid,nodev,noexec,relatime master:24 - cgroup cgroup rw,rdma
2645 2626 8:1 /var/lib/kubelet/pods/b34634ea-cd65-4099-bee5-ad4de9264ffb/volumes/kubernetes.io~empty-dir/temp-volume /tmp rw,relatime - ext4 /dev/sda1 rw,commit=30
2646 2628 0:234 / /dev/shm rw,relatime - tmpfs tmpfs rw,size=122209224k
2647 2626 8:1 /var/lib/kubelet/pods/b34634ea-cd65-4099-bee5-ad4de9264ffb/etc-hosts /etc/hosts rw,relatime - ext4 /dev/sda1 rw,commit=30
2648 2628 8:1 /var/lib/kubelet/pods/b34634ea-cd65-4099-bee5-ad4de9264ffb/containers/main/daf3b15a /dev/termination-log rw,relatime - ext4 /dev/sda1 rw,commit=30
2649 2626 8:1 /var/lib/containerd/io.containerd.grpc.v1.cri/sandboxes/207a0aae52b84a0b7a6eb703fd9db54fd97566c7f034e72ceb984e01471c8040/hostname /etc/hostname rw,nosuid,nodev,relatime - ext4 /dev/sda1 rw,commit=30
2650 2626 8:1 /var/lib/containerd/io.containerd.grpc.v1.cri/sandboxes/207a0aae52b84a0b7a6eb703fd9db54fd97566c7f034e72ceb984e01471c8040/resolv.conf /etc/resolv.conf rw,nosuid,nodev,relatime - ext4 /dev/sda1 rw,commit=30
2313 2627 0:292 /bus /proc/bus ro,nosuid,nodev,noexec,relatime - proc proc rw
2314 2627 0:292 /fs /proc/fs ro,nosuid,nodev,noexec,relatime - proc proc rw
2315 2627 0:292 /irq /proc/irq ro,nosuid,nodev,noexec,relatime - proc proc rw
2316 2627 0:292 /sys /proc/sys ro,nosuid,nodev,noexec,relatime - proc proc rw
2317 2627 0:292 /sysrq-trigger /proc/sysrq-trigger ro,nosuid,nodev,noexec,relatime - proc proc rw
2318 2627 0:296 / /proc/acpi ro,relatime - tmpfs tmpfs ro
2319 2627 0:293 /null /proc/kcore rw,nosuid - tmpfs tmpfs rw,size=65536k,mode=755
2320 2627 0:293 /null /proc/keys rw,nosuid - tmpfs tmpfs rw,size=65536k,mode=755
2321 2627 0:293 /null /proc/timer_list rw,nosuid - tmpfs tmpfs rw,size=65536k,mode=755
2322 2627 0:297 / /proc/scsi ro,relatime - tmpfs tmpfs ro
2323 2631 0:298 / /sys/firmware ro,relatime - tmpfs tmpfs ro
```

In this example we can see the line starting with 2641 contains our cgroup and has a mount point of `/sys/fs/cgroup/cpu,cpuacct`.

We then set `CGROUP_MOUNT` to regex match #6.

NOTE: ignore regex match #5, though in this example it happens to match `CGROUP_PATH`, there is no guarantee so you can't operate on `/proc/self/mountinfo` alone.

### Reading CPU limits

In order to read the CPU limits, read the file `${CGROUP_MOUNT}/${CGROUP_PATH}/cpu.cfs_quota_us` as `MAX`.  Read the file `${CGROUP_MOUNT}/${CGROUP_PATH}/cpu.cfs_period_us` as `PERIOD`.

### Calculate the number of CPUs

`MAX` may be -1, if so, this means unlimited (no throttle).

Otherwise, to get the number of allowed CPUs, divide `MAX` by `PERIOD`.  Round the result to the nearest integer.  If rounds to zero use 1.


