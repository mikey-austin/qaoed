# Introduction #

To be able to boot off a RAID assembled from several AoE devices it is necessary to modify initial ramdisk. The described process is done based on Debian Etch but may be easily ported to any other distribution.

The basic scenario used for this short tutorial is the following:
Two Xen servers exporting several logical volumes for their domUs. The servers have two NICs each where eth0 is the interface they use to provide public services and eth1 is used for AoE and XEN administration. The domUs should boot off a mirroring RAID that is created from two AoE devices -- one from each server. That way domUs might easily be (live) migrated between those two servers and one of the two might be shut down for maintenance and all domUs still exist on the other server. (RAIDs will have to be rebuilt afterwards, of course).

# Details #

To be able to modify the initial ramdisk we first need to take a working initrd apart:
```
# create a temp dir for modifying the initrd
mkdir /tmp/initrd-aoe ; cd /tmp/initrd-aoe
# extract initrd:
zcat /boot/initrd.img-2.6.18-4-xen-686 | cpio -i -d -H newc --no-absolute-filenames
```

The structure of the initial ramdisk:
```
/bin/         # binaries like busybox and the like
/lib/         # libs
/lib/modules/ # kernel modules; not all but most - aoe mod included
/conf/        # config files for initramfs, md, ...
/etc/         # system configs
/modules/     # (empty)
/sbin/        # module tools, mdadm & udev
/scripts/     # scripts needed to boot different options
/init         # the one and only... -> this is used to boot
```

So, up/down-grading the aoe module can be done
in lib/modules/

&lt;kernel-ver&gt;

/...

But main modifications are required in the init script to boot from aoe device
and/or create a raid from aoe...
First of we have to introduce some boot options to not have to create an
initrd per host:
aoe\_netdev - specify the network device to use for aoe
aoe\_md\_devs - devices to use for creating a raid (eg. "e0.0,e1.0" is assumed here)
The following patch can be used for parsing those arguments:
```
--- init        2007-01-20 14:12:57.000000000 +0100
+++ init        2007-01-20 14:29:38.000000000 +0100
@@ -87,6 +87,14 @@
        boot=*)
                BOOT=${x#boot=}
                ;;
+       aoe_netdev=*)
+               AOE_NETDEV="${x#aoe_netdev=}"
+               ;;
+       aoe_md_devs=*)
+               AOE_MD_DEV="${x#aoe_md_devs=}"
+               ;;
        resume=*)
                RESUME="${x#resume=}"
                ;;
```

now we will use those arguments:
(this should go after "run\_scripts /scripts/init-premount" in init)
```
# TODO: make this procedure failsafe
# TODO: move this into a separate script
if [ -n AOE_NETDEV ]; then
    # if AOE_NETDEV not specified we don't need to bring up aoe at all...
    # TODO: make module options configureable
    echo -n "loading aoe module..."
    modprobe aoe
    echo " done."
    echo -n "starting device ${AOE_NETDEV} for aoe..."
    ifconfig ${AOE_NETDEV} up
    sleep 3
    echo " done."
    echo -n "discovering AoE devices..."
    echo discover > /dev/etherd/discover
    sleep 2
    echo " done."
    # now check for raid...
    # TODO: make use of already existing raid infrastructure
    # for now this is all hardcoded (& kind of ugly)
    if [ -n AOE_MD_DEVS ]; then
        MD_DEVS=""
        for dev in $(echo $AOE_MD_DEVS | sed "s/,/\n/g"); do
            MD_DEVS="${MD_DEVS} /dev/etherd/${dev}"
        done
        echo "Using ${MD_DEVS} for creating /dev/md0"
        modprobe raid1
        mknod /dev/md0 b 9 0
        mdadm --assemble --run --force /dev/md0 ${AOE_MD_DEVS}
    fi
fi
```
now the script should continue with:
maybe\_break mount

Those modifications allow to create one initrd for all xen guests or even
standalone machines using a root device on AoE.
One only has to specify some options. Possible extensions could be to add
jumbo frames support if possible and to lower "aoe\_deadsecs" which brings
significant performance gains when one expects frequent downtimes of some
discs because the system will be near a complete freeze until the timeout has
been reached so that the device will be kicked out of the raid.

To recreate an initial ramdisk after modifying it, the following commands may be used:
```
find . -print | cpio -o -H newc | gzip > /boot/initrd.img-2.6.18-4-xen-aoe
```
watch out: I changed the name of the initial ramdisk!

now add the following to your domUs config in XEN (or in your lilo/grub conf if you don't use XEN at all):
```
kernel  = '/boot/vmlinuz-2.6.18-4-xen-686'
ramdisk = '/boot/initrd-2.6.18-4-xen-aoe'
memory  = '512'

root    = '/dev/md0 ro'
extra   = 'aoe_md_devs=e0.0,e1.0 aoe_netdev=eth1'

name    = 'hercules'

vif  = [ 'ip=10.10.1.5,mac=aa:00:00:00:04:00,bridge=xenbr0',
         'ip=192.168.1.5,mac=aa:00:00:00:04:01,bridge=xenbr1' ]
```

Feel free to experiment with different boot options and send in your own enhancements! As you can see there are lots of TODOs left... but the basic script is just working!

Notes:
  * This setup only worked with qaoed for me; vblade was unable to export the device in a way that the domU could "see" the devices exported by its dom0.
  * I gave up on this completely because I could not get qaoed work the way I wanted (see bug report about "-f") and decided that I lost enough time experimenting with ATA-over-Ethernet. I started my work in September 2006 and could not get it working the way I wanted up to now. Probably someone will pick that work up and make it just work. Probably I will check back in a year or two and continue using AoE...