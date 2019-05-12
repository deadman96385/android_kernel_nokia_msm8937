#cd ../../
#source build/envsetup.sh
#lunch msm8937_64-userdebug
#cd -

export GCC_TOOLS=${PWD}/../../prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin/
export PATH=${GCC_TOOLS}:$PATH
fih_project=msm8937
project=msm8937_32go
echo project = $project
echo fih_project = $fih_project


make -j16 -C ../../out/target/product/$project/obj/KERNEL_OBJ ARCH=arm CROSS_COMPILE=arm-linux-androideabi-  KCFLAGS=-mno-android ${fih_project}_defconfig
make -j16 -C ../../out/target/product/$project/obj/KERNEL_OBJ ARCH=arm CROSS_COMPILE=arm-linux-androideabi-  KCFLAGS=-mno-android
cp ../../out/target/product/$project/obj/KERNEL_OBJ/arch/arm/boot/zImage-dtb ../../out/target/product/$project/kernel
cd ../../
#make -j4 ramdisk
make -j4 bootimage-nodeps
