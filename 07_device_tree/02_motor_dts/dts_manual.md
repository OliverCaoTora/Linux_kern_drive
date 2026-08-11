# 1. Enter Linux kernal
cd /100ask_imx6ull-sdk/Linux-4.9.88/

# 2. change dts file
vi arch/arm/boot/dts/100ask_imx6ull-14x14.dts

# 3. add device node
Under "/" root node:

motor {
    compatible = "100ask, gpiodemo";
    gpios = <&gpio4 19 GPIO_ACTIVE_HIGH>,
            <&gpio4 20 GPIO_ACTIVE_HIGH>,
            <&gpio4 21 GPIO_ACTIVE_HIGH>,
            <&gpio4 22 GPIO_ACTIVE_HIGH>;
};

# 4. Build dtbs
make dtbs

# 5. copy into board "boot" index
cp arch/arm/boot/dts/100ask_imx6ull-14x14.dtb XXX/boot

# 6. reboot