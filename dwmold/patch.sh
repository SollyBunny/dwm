#!/bin/sh
if [ -z "$1" ]; then
	echo "Applying all patches"
	read -r _
    while IFS= read -r line || [ -n "$line" ]; do
        # Skip empty lines and lines starting with #
        if [ -z "$line" ] || [ "${line#"#"}" != "$line" ]; then
            continue
        fi
        # Run ./patch.sh with the line as an argument
        ./patch.sh "$line"
    done < "patches"
    exit 0
fi
cd dwm
for file in *.rej *.orig; do
    # Check if the file exists
    if [ -f "$file" ]; then
        # Move the file to _<filename>
        mv "$file" "_$file"
        echo "Moved $file to _$file"
    fi
done
curl "$1" | patch