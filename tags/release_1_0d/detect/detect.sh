#!/bin/sh
#
# This scripts scans for installed versions of Loki games, for setupdb

# Find where the md5sum program is
cwd=`pwd`
md5="$cwd/md5sum"

# Get to the correct data directory
cd "`dirname $0`"

# Check command line
if [ "$1" = "" ]; then
    echo "Usage: $0 <product>" >&2
    exit 1
fi
product=$1

# If there's a special script, use that
if [ -f "$product.sh" ]; then
    status=2
    . "$product.sh"
    exit $status
fi

# Otherwise, perform a standard algorithm that catches most cases
if [ ! -f "$product.md5" ]; then
    echo "No MD5 checksum file for $product" >&2
    exit 2
fi
product_desc=`tail +2 "$product.md5" | head -n 1`
product_url=`tail +3 "$product.md5" | head -n 1`
for product_bin in `tail +1 "$product.md5" | head -n 1`
do  for path in "$UPDATE_CWD" \
                /opt \
                /opt/games \
                /usr/games \
                /usr/local/games \
                "$HOME/games" \
                "$HOME"
    do  if [ ! -d "$path" ]; then
            continue
        fi
        binary=`ls -d "$path/$product_bin" 2>/dev/null | head -n 1`
        if [ ! -f "$binary" ]; then
            binary=`ls -d "$path"/*/"$product_bin" 2>/dev/null | head -n 1`
        fi 
        if [ -f "$binary" ]; then
            # Is the awk/ls magic portable?
            if [ -L "$binary" ]; then
                binary=`ls -l "$binary" | awk '{print $11}'`
            fi
            product_path=`dirname $binary`
            if [ -w "$product_path" ]; then
                set -- `"$md5" "$binary"`
                sum="$1"
                tail +4 "$product.md5" | while read line
                do  set -- $line
                    if [ "$sum" = "$1" ]; then
                        # Woohoo!  We found it!
                        product_version=$2
                        echo "$product_version"
                        echo "$product_desc"
                        echo "$product_path"
                        echo "$product_url"
                        exit 0
                    fi
                done
            fi
        fi
    done
done
exit 2
