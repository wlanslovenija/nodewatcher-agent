nodewatcher-agent
=================

Nodewatcher agent is a monitoring agent that runs on OpenWrt-supported devices and
can provide various telemetry information to the nodewatcher_ monitor daemon via
HTTP.

.. _nodewatcher: http://nodewatcher.net/

Configuration
-------------

The agent may be configured via UCI. An example configuration, also used by the
provided `OpenWrt package`_, which should be placed under ``/etc/config/nodewatcher``
follows below::

  config agent
    option output_json '/www/nodewatcher/feed'

Currently, the only option is to configure where the JSON output feed should be placed. By
default, no output feed is generated and nodewatcher agent data is only accessible via
the ubus API.

.. _OpenWrt package: https://github.com/wlanslovenija/firmware-packages-opkg/tree/master/util/nodewatcher-agent

ubus API
--------

The nodewatcher agent exposes an API via ubus_ so other applications can access its
data feeds. It registers itself under the `nodewatcher.agent` identifier. Currently
the only supported method is ``get_data`` which can be used as follows::

  $ ubus call nodewatcher.agent get_data
  {
    ...
    // Returns data feed in the format described below

The method accepts a single parameter called ``module`` which can be used to limit the
output to a specific module as follows::

  $ ubus call nodewatcher.agent get_data "{ 'module': 'core.general' }"

.. _ubus: http://wiki.openwrt.org/doc/techref/ubus

Monitoring report format
------------------------

To output monitoring data, JSON format is used as in the following example:

.. code:: javascript

  {
    // Each module outputs a section that has the module name as the section key.
    'core.general': {
      // Inside the section a special _meta section is always present, giving
      // some metadata about the module. Currently, the only metadata is the
      // module's version number.
      '_meta': {
        'version': 4
      },
      // Additional sections are module-dependent and contain monitoring data.
      'uuid': '64840ad9-aac1-4494-b4d1-9de5d8cbedd9',
      'hostname': 'test-4',
      'version': 'git.12f427d',
      'kernel': '3.10.36',
      'local_time': 1401093621,
      'uptime': 4612,
      'hardware': {
        'board': 'tl-wr741nd-v4',
        'model': 'TP-Link TL-WR740N/ND v4'
      }
    }
  }

All float values are encoded as strings due to the fact that ubus message blobs do
not currently support serialization of float/double types.

HTTP push support
-----------------

The agent can also be configured to perform periodic push of monitoring data by using HTTP
POST requests. This functionality is implemented in the ``http_push`` module which must
be enabled for this to be available. The use of this module requires ``libcurl`` to be
installed.

After enabling the module, the following additional options may be specified via UCI::

  config agent
    # ...

    # Push URL.
    option push_url 'https://host/push/http/64840ad9-aac1-4494-b4d1-9de5d8cbedd9'
    # Push interval in seconds.
    option push_interval '120'
    # Path to server-side public key for authenticating the server.
    option push_server_pubkey '/etc/crypto/public_key/server'

Push is performed via a single HTTP POST request to the specified URL where the body contains
the same JSON-formatted document as is used for reports.

Modules
-------

The agent is fully modular, with all reporting functionality being implemented in
modules which are loaded as shared library plugins. On startup modules are automatically
discovered from ``/usr/lib/nodewatcher-agent``. Currently the following modules are
implemented:

* ``core.general`` provides general information about the running system such as the node's uuid, hostname, kernel and firmware versions, etc.

* ``core.resources`` provides system resource usage information such as the amount of memory used, the number and type of running processes, load averages, CPU usage and number of tracker connections.

* ``core.interfaces`` reports status and statistics for network interfaces configured via UCI.

* ``core.wireless`` provides additional information for wireless interfaces.

* ``core.keys.ssh`` provides information about the node's host SSH keys.

* ``core.clients`` provides information about the clients currently connected with the node, obtained from DHCP leases file.

* ``core.push.http`` enables periodic push of JSON data to a remote nodewatcher server.

* ``core.routing.babel`` provides information about the node's Babel routing daemon.

* ``core.routing.olsr`` provides information about the node's OLSR routing daemon.

* ``core.meshpoint`` provides information about the Cilab MeshPoint node's built-in sensors.

Development setup
-----------------

To build the master version of nodewatcher agent package a buildroot needs to be set up. It can either be built from OpenWrt source or a precompiled OpenWrt SDK can be used.
This example uses 18.06.1 version of OpenWrt, ipq40xx target and its SDK. For latest version `OpenWrt download page`_ should be checked.::

  curl -O https://downloads.openwrt.org/releases/18.06.1/targets/ipq40xx/generic/openwrt-sdk-18.06.1-ipq40xx_gcc-7.3.0_musl_eabi.Linux-x86_64.tar.xz
  tar xvf openwrt-sdk-18.06.1-ipq40xx_gcc-7.3.0_musl_eabi.Linux-x86_64.tar.xz
  cd openwrt-sdk-18.06.1-ipq40xx_gcc-7.3.0_musl_eabi.Linux-x86_64

Now you can either manually add Makefile localy or add a remote repository.
In this example a remote repository is used.::
  echo "src-git wlansi https://github.com/wlanslovenija/firmware-packages-opkg.git" >> feeds.conf.default
  ./scripts/feeds update -a
  ./scripts/feeds install nodewatcher-agent

You probably want to disable some default settings, which build every available package. Enter Global Build Settings and in the submenu, deselect/exclude the following options:
::
  Select all target specific packages by default
  Select all kernel module packages by default
  Select all userspace packages by default

After that go to Base system and select Nodewatcher-agent and any modules you want.
And simple make V=s will start building the package.

Installing packages
~~~~~~~~~~~~~~~~~~~

If building for an ``x86_64`` architecture, a virtual machine can be used for development. You can acquire VMWare or Virtualbox images from `OpenWrt download page`_.
Once the VM is running, set the SSH password using ``passwd`` and change ``lan`` setting from ``static`` to ``dhcp`` using::

  uci set network.lan.proto='dhcp'
  uci commit
  reboot

The built packages are located in ``bin/packages/``. They need to be transferred to a target device or virtual machine and installed. The easiest way would be to set up a local network and transfer the packages using::

  scp $filename root@192.168.X.X:/tmp

Once all the packages are on the target machine connect to it using SSH::

  ssh root@192.168.X.X

Go to target directories and install packages using::

  opkg install *.ipk

Troubleshooting
~~~~~~~~~~~~~~~

Troubleshoot connection issues with commands like ``uci show dropbear``, ``cat /etc/passwd``, ``cat /etc/shadow``, ``logread``, ``ip addr``...

.. _firmware core: https://github.com/wlanslovenija/firmware-core
.. _OpenWrt download page: https://downloads.openwrt.org

Source Code, Issue Tracker and Mailing List
-------------------------------------------

For development Github issues is used, so you can see `existing open tickets`_ or `open a new one`_ there. 
Source code is available on GitHub_.

.. _existing open tickets: https://github.com/wlanslovenija/nodewatcher-agent/issues
.. _open a new one: https://github.com/wlanslovenija/nodewatcher-agent/issues/new
.. _GitHub: https://github.com/wlanslovenija/nodewatcher-agent
