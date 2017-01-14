#
# This spec file is used to build the test*.rpm package we use in one of
# our tests.
#
# The advantage of using our own custom package, instead of downloading a
# random one from the net, is that we get the chance here to define all the
# tags our rpm helper is supposed to support.
#
# Build this package with:
#
#    $ rpmbuild -bb test.spec
#
# Then create the input for the test with:
#
#    $ perl /path/to/rpm2tags.pl ~/rpmbuild/RPMS/noarch/test*.rpm > rpm.custom.input
#
Name:           test
Summary:        Testing
Epoch:          1
Version:        2.3
Release:        4%{?dist}
URL:            http://example.com
Group:          Development/System
License:        MIT
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root
BuildArch:      noarch
Conflicts:      notest
Obsoletes:      testing
Distribution:   Test Distro
Packager:       Test Packager
Vendor:         Test Vendor


%description
Multi-line description field
with "double", 'single quotes', and $weird | \characters i\n = i\\t, empty line...

...and a tab: [	].

%install
[ "%{buildroot}" != / ] && %{__rm} -rf "%{buildroot}"
%{__mkdir_p} %{buildroot}%{_tmppath}
echo %{name} > %{buildroot}%{_tmppath}/%{name}.txt


%pretrans
echo "Pre-transaction script"


%pre
echo "Pre-installation script"


%post
echo "Post-installation script"


%preun
echo "Pre-uninstallation script"


%postun
echo "Post-uninstallation script"


%posttrans
echo "Post-transaction script"


%verifyscript
echo "Verify script"


%clean
[ "%{buildroot}" != / ] && %{__rm} -rf "%{buildroot}"


%files
%defattr(-,root,root,-)
%{_tmppath}/%{name}.txt


%changelog
* Fri Dec 30 2016 Jiri Tyr <jiri.tyr at gmail.com> 1:2.3-4
- Initial build.
