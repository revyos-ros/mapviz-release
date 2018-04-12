Name:           ros-kinetic-mapviz
Version:        0.2.5
Release:        0%{?dist}
Summary:        ROS mapviz package

Group:          Development/Libraries
License:        BSD
URL:            https://github.com/swri-robotics/mapviz
Source0:        %{name}-%{version}.tar.gz

Requires:       freeglut-devel
Requires:       glew-devel
Requires:       libXi-devel
Requires:       libXmu-devel
Requires:       qt5-qtbase
Requires:       qt5-qtbase-gui
Requires:       ros-kinetic-cv-bridge
Requires:       ros-kinetic-image-transport
Requires:       ros-kinetic-marti-common-msgs
Requires:       ros-kinetic-message-runtime
Requires:       ros-kinetic-pluginlib
Requires:       ros-kinetic-rosapi
Requires:       ros-kinetic-roscpp
Requires:       ros-kinetic-rqt-gui
Requires:       ros-kinetic-rqt-gui-cpp
Requires:       ros-kinetic-std-srvs
Requires:       ros-kinetic-swri-transform-util
Requires:       ros-kinetic-swri-yaml-util
Requires:       ros-kinetic-tf
BuildRequires:  freeglut-devel
BuildRequires:  glew-devel
BuildRequires:  libXi-devel
BuildRequires:  libXmu-devel
BuildRequires:  pkgconfig
BuildRequires:  qt5-qtbase
BuildRequires:  qt5-qtbase-gui
BuildRequires:  ros-kinetic-catkin
BuildRequires:  ros-kinetic-cv-bridge
BuildRequires:  ros-kinetic-image-transport
BuildRequires:  ros-kinetic-marti-common-msgs
BuildRequires:  ros-kinetic-message-generation
BuildRequires:  ros-kinetic-pluginlib
BuildRequires:  ros-kinetic-rosapi
BuildRequires:  ros-kinetic-roscpp
BuildRequires:  ros-kinetic-rqt-gui
BuildRequires:  ros-kinetic-rqt-gui-cpp
BuildRequires:  ros-kinetic-std-srvs
BuildRequires:  ros-kinetic-swri-transform-util
BuildRequires:  ros-kinetic-swri-yaml-util
BuildRequires:  ros-kinetic-tf

%description
mapviz

%prep
%setup -q

%build
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree that was dropped by catkin, and source it.  It will
# set things like CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/kinetic/setup.sh" ]; then . "/opt/ros/kinetic/setup.sh"; fi
mkdir -p obj-%{_target_platform} && cd obj-%{_target_platform}
%cmake .. \
        -UINCLUDE_INSTALL_DIR \
        -ULIB_INSTALL_DIR \
        -USYSCONF_INSTALL_DIR \
        -USHARE_INSTALL_PREFIX \
        -ULIB_SUFFIX \
        -DCMAKE_INSTALL_LIBDIR="lib" \
        -DCMAKE_INSTALL_PREFIX="/opt/ros/kinetic" \
        -DCMAKE_PREFIX_PATH="/opt/ros/kinetic" \
        -DSETUPTOOLS_DEB_LAYOUT=OFF \
        -DCATKIN_BUILD_BINARY_PACKAGE="1" \

make %{?_smp_mflags}

%install
# In case we're installing to a non-standard location, look for a setup.sh
# in the install tree that was dropped by catkin, and source it.  It will
# set things like CMAKE_PREFIX_PATH, PKG_CONFIG_PATH, and PYTHONPATH.
if [ -f "/opt/ros/kinetic/setup.sh" ]; then . "/opt/ros/kinetic/setup.sh"; fi
cd obj-%{_target_platform}
make %{?_smp_mflags} install DESTDIR=%{buildroot}

%files
/opt/ros/kinetic

%changelog
* Thu Apr 12 2018 Marc Alban <malban@swri.org> - 0.2.5-0
- Autogenerated by Bloom

* Fri Aug 11 2017 Marc Alban <malban@swri.org> - 0.2.4-0
- Autogenerated by Bloom

* Sat Dec 10 2016 Marc Alban <malban@swri.org> - 0.2.3-0
- Autogenerated by Bloom

* Wed Dec 07 2016 Marc Alban <malban@swri.org> - 0.2.2-0
- Autogenerated by Bloom

* Thu Jun 23 2016 Marc Alban <malban@swri.org> - 0.2.0-0
- Autogenerated by Bloom

