#!/bin/sh
#
# This scripts scans for installed versions of CivCTP, for setupdb

# Otherwise, perform a standard algorithm that catches most cases
if [ ! -f "$product.md5" ]; then
    echo "No MD5 checksum file for $product" >&2
    exit 2
fi
product_bin=`tail +1 "$product.md5" | head -n 1`
product_desc=`tail +2 "$product.md5" | head -n 1`
product_url=`tail +3 "$product.md5" | head -n 1`
for path in "$UPDATE_CWD" \
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
                    product_version="$2"
                    if echo $product_version | grep "-" >/dev/null 2>&1; then
                        echo "$product_version"
                    else
                        product_extension=`tail +4 "$product_path/civpaths.txt" | head -n 1 | tr -d '\r'`
                        echo "$product_version-$product_extension"
                    fi
                    echo "$product_desc"
                    echo "$product_path"
                    echo "$product_url"
                    exit 0
                fi
            done
        fi
    fi
done
exit 2
