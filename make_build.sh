#!/bin/bash
#
# make_build.sh
# Copyright 2021 Ayn Inc.
#


CUR_PATH=$(pwd)
#CUR_PATH=`pwd`
echo ${CUR_PATH}
BUILD_OUT_PLATFORM_QSSI=qssi
BUILD_OUT_PLATFORM_SDM845=sdm845

COPY_OUT_PATH=build_out
if [ ! -d "$COPY_OUT_PATH" ];then
	echo "需要创建"$COPY_OUT_PATH
	mkdir $COPY_OUT_PATH
fi

funUserDebugChange(){
	echo "User Debug Change, need clean part of the out file! "
	if [ x"$1" == "x" ] ;then
		echo "$1 is null! "
		return;
	fi
	rm -rf out/target/product/$1/obj_arm/EXECUTABLES/
	rm -rf out/target/product/$1/obj_arm/STATIC_LIBRARIES/

	echo "$1 生成新的版本号!!!"
	rm -rf `find out/target/product/ -name "build.prop"`
	rm -rf out/target/product/$1/system*
	rm -rf out/target/product/$1/vendor/
	rm -rf out/target/product/$1/product/
	rm -rf out/target/product/$1/obj/APPS/
	rm -rf out/target/product/$1/obj/JAVA_LIBRARIES/
	rm -rf out/target/product/$1/obj/ETC/
	rm -rf out/target/product/$1/obj/EXECUTABLES/
	rm -rf out/target/product/$1/obj/DATA/
	rm -rf out/target/product/$1/obj/STATIC_LIBRARIES/
	rm -rf out/target/product/$1/obj/CONFIG/
}

funCleanKernel(){
	echo "清空kernel !!!"
	cd kernel/msm-4.9
	make mrproper
	cd ${CUR_PATH}
}

funCopyBuildFile(){
	echo "拷贝编译文件 !!!"
	mv out/dist/merged-qssi_sdm845-target_files.zip $COPY_OUT_PATH
	mv out/dist/merged-qssi_sdm845-ota.zip $COPY_OUT_PATH
}



PRODUCT_M2=M2
PRODUCT_M0=M0
PRODUCT_ONEStore=ONEStore
funProductSelection(){
	if [ -f $COPY_OUT_PATH/$PRODUCT_M2 ];then
		echo "当前编译环境：${PRODUCT_M2}"

	elif [ -f $COPY_OUT_PATH/$PRODUCT_ONEStore ];then
		echo "当前编译环境：${PRODUCT_ONEStore}"

	elif [ -f $COPY_OUT_PATH/$PRODUCT_M0 ];then
		echo "当前编译环境：${PRODUCT_M0}"

	else
		echo "当前编译环境未选择"
	fi
	echo "选择产品型号进行编译 !!!"
	./product_selection.sh
	PRODUCT_TYPE_CHANGE=$?
	echo "PRODUCT_TYPE_CHANGE=${PRODUCT_TYPE_CHANGE}"
	if [ $PRODUCT_TYPE_CHANGE -eq 1 ]; then
		funCleanKernel
		funUserDebugChange $BUILD_OUT_PLATFORM_QSSI
		funUserDebugChange $BUILD_OUT_PLATFORM_SDM845
	fi
}

# 产品类型选择
if [ x"$1" == "xPS" ] ;then
	funProductSelection
	exit
fi

# 还没有编译过，或者清空过时，需要选择一款产品进行编译
if [ ! -f $COPY_OUT_PATH/$PRODUCT_M2 ];then
	if [ ! -f $COPY_OUT_PATH/$PRODUCT_M0 ];then
		if [ ! -f $COPY_OUT_PATH/$PRODUCT_ONEStore ];then
			funProductSelection
			exit
		fi
	fi
fi


if [ -f $COPY_OUT_PATH/$PRODUCT_M2 ];then
	echo "正在编译${PRODUCT_M2}..."
fi
if [ -f $COPY_OUT_PATH/$PRODUCT_ONEStore ];then
	echo "正在编译${PRODUCT_ONEStore}..."
fi
if [ -f $COPY_OUT_PATH/$PRODUCT_M0 ];then
	echo "正在编译${PRODUCT_M0}..."
fi

if [ x"$1" == "xCP" ] ;then
	if [ -f $COPY_OUT_PATH/$PRODUCT_M2 ];then
		cp -rf $PRODUCT_M2/* .
	fi
	if [ -f $COPY_OUT_PATH/$PRODUCT_ONEStore ];then
		cp -rf $PRODUCT_ONEStore/* .
	fi
	if [ -f $COPY_OUT_PATH/$PRODUCT_M0 ];then
		cp -rf $PRODUCT_M0/* .
	fi
	exit
fi

#清除 build.prop
if [ x"$1" == "xBPC" ] ;then
	echo "生成新的版本号 !!!"
	funUserDebugChange $BUILD_OUT_PLATFORM_QSSI
	funUserDebugChange $BUILD_OUT_PLATFORM_SDM845
	exit
fi

#查看build.prop中的版本号
if [ x"$1" == "xV" ] ;then
	echo "查看版本号 !!!"
	cat out/target/product/qssi/system/build.prop | grep build
	exit
fi

#dtbo分区烧录dtbo.img
if [ x"$1" == "xDT" ] ;then
	echo "编译DTS文件 !!!"
	source build/envsetup.sh
	#lunch sdm845-userdebug
	lunch sdm845-user
	make dtboimage
	exit
fi


if [ x"$1" == "xKC" ] ;then
	funCleanKernel
	exit
fi

if [ x"$1" == "xKU" ] ;then
	echo "编译kernel !!!"
	source build/envsetup.sh
	#lunch sdm845-userdebug
	lunch sdm845-user
	make bootimage -j64
	#boot_a分区烧录boot.img
	exit
fi

if [ x"$1" == "xKD" ] ;then
	echo "编译kernel !!!"
	source build/envsetup.sh
	lunch sdm845-userdebug
	make bootimage -j64
	#boot_a分区烧录boot.img
	exit
fi

if [ x"$1" == "xINC" ] ;then
	echo "编译增量包 !!!"
	source build/envsetup.sh
	lunch sdm845-user
	./build/tools/releasetools/ota_from_target_files -v --block -p out/host/linux-x86 -k build/target/product/security/releasekey -i $COPY_OUT_PATH/v1/merged-qssi_sdm845-target_files.zip $COPY_OUT_PATH/v2/merged-qssi_sdm845-target_files.zip update.zip
	exit
fi


if [ x"$1" == "xAUO" ] ;then
	echo "编译 user OTA !!!"
	source build/envsetup.sh
	lunch sdm845-user
	./build.sh dist -j96
	funCopyBuildFile
fi

if [ x"$1" == "xADO" ] ;then
	echo "编译 userdebug OTA !!!"
	source build/envsetup.sh
	lunch sdm845-userdebug
	./build.sh dist -j96
	funCopyBuildFile
fi

if [ x"$1" == "xAU" ] ;then
	echo "编译USER版本"
	source build/envsetup.sh
	lunch sdm845-user
	./build.sh -j96
fi

if [ x"$1" == "xAD" ] ;then
	echo "编译DEBUG版本"
	source build/envsetup.sh
	lunch sdm845-userdebug
	./build.sh -j96
fi

BURN_PATH=${CUR_PATH}/../../burn/Unpacking_Tool_2021_develop/LINUX/android/out/target/product/sdm845/
#echo ${CUR_PATH}
#echo ${BURN_PATH}
cp out/target/product/sdm845/abl.elf ${BURN_PATH}
#cp out/target/product/sdm845/abl.img ${BURN_PATH}/abl.elf
cp device/qcom/sdm845/radio/xbl.img ${BURN_PATH}/xbl.elf

cp out/target/product/sdm845/boot.img ${BURN_PATH}
cp out/target/product/sdm845/dtbo.img ${BURN_PATH}
cp out/target/product/sdm845/persist.img ${BURN_PATH}
cp out/target/product/sdm845/system.img ${BURN_PATH}
cp out/target/product/sdm845/vendor.img ${BURN_PATH}
cp out/target/product/sdm845/vbmeta.img ${BURN_PATH}
cp out/target/product/sdm845/userdata.img ${BURN_PATH}

cp out/target/product/sdm845/obj/KERNEL_OBJ/vmlinux ${BURN_PATH}/obj/KERNEL_OBJ/

cp out/target/product/sdm845/vendor/firmware/a630_zap.elf ${BURN_PATH}/vendor/firmware/
