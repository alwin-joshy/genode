/*
 * \brief  Sculpt Wifi-driver management
 * \author Norman Feske
 * \date   2024-03-26
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER__WIFI_H_
#define _DRIVER__WIFI_H_

namespace Sculpt { struct Wifi_driver; }


struct Sculpt::Wifi_driver : private Noncopyable
{
	Constructible<Child_state> _wifi { };

	void gen_start_node(Xml_generator &xml) const
	{
		if (!_wifi.constructed())
			return;

		xml.node("start", [&] {
			_wifi->gen_start_node_content(xml);
			gen_named_node(xml, "binary", "wifi");

			xml.node("config", [&] {
				xml.attribute("dtb", "wifi.dtb");

				xml.node("vfs", [&] {
					gen_named_node(xml, "dir", "dev", [&] {
						xml.node("null", [&] {});
						xml.node("zero", [&] {});
						xml.node("log",  [&] {});
						xml.node("null", [&] {});
						gen_named_node(xml, "jitterentropy", "random");
						gen_named_node(xml, "jitterentropy", "urandom"); });
						gen_named_node(xml, "inline", "rtc", [&] {
							xml.append("2018-01-01 00:01");
						});
					gen_named_node(xml, "dir", "firmware", [&] {
						xml.node("tar", [&] {
							xml.attribute("name", "wifi_firmware.tar");
						});
					});
				});

				xml.node("libc", [&] {
					xml.attribute("stdout", "/dev/log");
					xml.attribute("stderr", "/dev/null");
					xml.attribute("rtc",    "/dev/rtc");
				});
			});

			xml.node("route", [&] {
				gen_service_node<Platform::Session>(xml, [&] {
					xml.node("parent", [&] {
						xml.attribute("label", "wifi"); }); });
				xml.node("service", [&] {
					xml.attribute("name", "Uplink");
					xml.node("child", [&] {
						xml.attribute("name", "nic_router");
						xml.attribute("label", "wifi -> "); }); });
				gen_common_routes(xml);
				gen_parent_rom_route(xml, "wifi");
				gen_parent_rom_route(xml, "wifi.dtb");
				gen_parent_rom_route(xml, "libcrypto.lib.so");
				gen_parent_rom_route(xml, "vfs.lib.so");
				gen_parent_rom_route(xml, "libc.lib.so");
				gen_parent_rom_route(xml, "libm.lib.so");
				gen_parent_rom_route(xml, "vfs_jitterentropy.lib.so");
				gen_parent_rom_route(xml, "libssl.lib.so");
				gen_parent_rom_route(xml, "wifi.lib.so");
				gen_parent_rom_route(xml, "wifi_firmware.tar");
				gen_parent_rom_route(xml, "wpa_driver_nl80211.lib.so");
				gen_parent_rom_route(xml, "wpa_supplicant.lib.so");
				gen_parent_route<Rm_session>   (xml);
				gen_parent_route<Rtc::Session> (xml);
				gen_service_node<Rom_session>(xml, [&] {
					xml.attribute("label", "wifi_config");
					xml.node("parent", [&] {
						xml.attribute("label", "config -> managed/wifi"); }); });
			});
		});
	};

	void update(Registry<Child_state> &registry, Board_info const &board_info)
	{
		bool const use_wifi = board_info.wifi_avail()
		                  &&  board_info.options.wifi
		                  && !board_info.options.suspending;

		_wifi.conditional(use_wifi, registry, "wifi", Priority::DEFAULT,
		                  Ram_quota { 16*1024*1024 }, Cap_quota { 260 });
	}
};

#endif /* _DRIVER__WIFI_H_ */
