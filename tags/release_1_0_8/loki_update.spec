%define name loki_update
%define version 1.0.8
%define release 1

Summary: Loki Update Tool
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: GPL
Group: Applications
Vendor: Loki Software, Inc.
Packager: Sam Lantinga <hercules@lokigames.com>

%description
This is a tool written by Loki Software, Inc., designed to be used in
conjunction with their setup and patchkit tools to automatically update
installed products.

%post
for dir in "$HOME/.loki" "$HOME/.loki/installed"
do test -d "$dir" || mkdir "$dir"
done
ln -sf /usr/local/Loki_Update/.manifest/Loki_Update.xml $HOME/.loki/installed

%postun
rm -f $HOME/.loki/installed/Loki_Update.xml

%files
/usr/local/Loki_Update/
/usr/local/bin/loki_update

%changelog
* Fri Dec 8 2000 Sam Lantinga <hercules@lokigames.com>
- First attempt at a spec file for loki_update

# end of file
