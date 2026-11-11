# /etc/profile.d/home-path.sh - put /home on PATH, so `tamacore` runs by name.
#
# Installed by scripts/install-tamacore.sh. Do not edit on the watch: /etc is
# jffs2 and survives a kernel-only flash, but a FULL image flash rewrites it,
# so this file's home is the repo.
#
# Why PATH and not a symlink into /usr/bin: / is cramfs and read-only. /home is
# jffs2 and writable, and it is where deploys already put binaries, so /home is
# the only place an application can actually live on this board.
#
# Appended, never prepended. Nothing in /home should be able to shadow a system
# binary -- on a root shell that would be a quiet way to replace `ls`.
case ":$PATH:" in
*:/home:*) ;;
*) export PATH="$PATH:/home" ;;
esac
