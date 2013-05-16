#Use this script *once per node* to set up the driver for this git instance.
#You will also need the .gitattributes and keepMine.sh files that are part of the timing branch.
git config merge.keepMine.name "always keep mine during merge"
git config merge.keepMine.driver "keepMine.sh %O %A %B"
