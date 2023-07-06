proc platform_supported { } {
	if {[have_spec x86_64] && [have_board pc]} {
		if {![have_spec linux] && ![have_spec foc] && ![have_spec sel4]} {
			return 1 } }
	return 0
}

if {![platform_supported]} {
	puts "Run script is not supported on this platform"
	exit 0
}

build { core lib/ld init timer monitor drivers/uart test/log }

create_boot_directory

install_config {
<config>
	<parent-provides>
		<service name="LOG"/>
		<service name="PD"/>
		<service name="CPU"/>
		<service name="ROM"/>
		<service name="IRQ"/>
		<service name="IO_MEM"/>
		<service name="IO_PORT"/>
		<service name="RM"/>
	</parent-provides>
	<default-route>
		<any-service> <parent/> <any-child/> </any-service>
	</default-route>
	<default caps="100"/>

	<start name="timer">
		<resource name="RAM" quantum="1M"/>
		<provides> <service name="Timer"/> </provides>
	</start>

	<start name="pc_uart_drv">
		<resource name="RAM" quantum="2M"/>
		<provides>
			<service name="Terminal"/>
			<service name="Uart"/>
		</provides>
		<config>
			<policy label_prefix="monitor" uart="1"/>
		</config>
	</start>

	<start name="monitor" caps="1000">
		<resource name="RAM" quantum="100M"/>
		<config>
			<parent-provides>
				<service name="LOG"/>
				<service name="PD"/>
				<service name="CPU"/>
				<service name="ROM"/>
			</parent-provides>
			<default caps="100"/>

			<monitor>
				<policy label="first-test-log" wx="yes"/>
			</monitor>

			<start name="first-test-log">
				<resource name="RAM" quantum="2M"/>
				<binary name="test-log"/>
				<route>
					<service name="PD">  <local/> </service>
					<service name="CPU"> <local/> </service>
					<any-service> <parent/> </any-service>
				</route>
			</start>

			<start name="second-test-log">
				<resource name="RAM" quantum="2M"/>
				<binary name="test-log"/>
				<route>
					<service name="PD">  <local/> </service>
					<service name="CPU"> <local/> </service>
					<any-service> <parent/> </any-service>
				</route>
			</start>

		</config>
	</start>
</config>
}

build_boot_image [build_artifacts]

set local_port 5555

# qemu config
append qemu_args " -display none "

# connect comport 0 to stdio
append qemu_args " -serial stdio "

# connect comport 1 with TCP port $local_port
append qemu_args " -serial chardev:uart "
append qemu_args " -chardev socket,id=uart,port=$local_port,host=localhost,server,nowait,ipv4 "

run_genode_until {.*\[init -> monitor -> first-test-log\].*} 30
set genode_id [output_spawn_id]