nodewatcher-agent
=================

Nodewatcher agent is a monitoring agent that runs on OpenWrt-supported devices and
can provide various telemetry information to the nodewatcher_ monitor daemon via
HTTP.

.. _nodewatcher: https://github.com/wlanslovenija/nodewatcher

ubus API
--------

The nodewatcher agent exposes an API via ubus_ so other applications can access its
data feeds. It registers itself under the `nodewatcher.agent` identifier. Currently
the only supported method is `get_data` which can be used as follows::

  $ ubus call nodewatcher.agent get_data
  {
    ...
    // Returns data feed in the format described below

The method accepts a single parameter called `module` which can be used to limit the
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
      'local_time': 1401093621,
      'uptime': 4612,
      'machine': 'TP-LINK TL-WR1043ND'
    }
  }
