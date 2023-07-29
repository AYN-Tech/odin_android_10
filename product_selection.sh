#!/bin/bash

PRODUCT_M2=M2
PRODUCT_M0=M0
PRODUCT_ONEStore=ONEStore
PRODUCT_DEFAULT=unknown

COPY_OUT_PATH=build_out
if [ ! -d "$COPY_OUT_PATH" ];then
	echo "需要创建"$COPY_OUT_PATH
	mkdir $COPY_OUT_PATH
fi


funProductSelect(){
	echo "请选择产品类型："
	echo "1、${PRODUCT_M0}"
	echo "2、${PRODUCT_M2}"
	echo "3、${PRODUCT_ONEStore}"

	read -p "请输入数字1、2、3：" input
	echo #相当于换行
	if [ ! -z $input ]; then
		input=`echo $input | tr [a-z] [A-z]`
	fi
	case $input in
		1)
		PRODUCT_DEFAULT=$PRODUCT_M0
		echo '你选择了：1、'$PRODUCT_DEFAULT
		;;

		2)
		PRODUCT_DEFAULT=$PRODUCT_M2
		echo '你选择了：2、'$PRODUCT_DEFAULT
		;;

		3)
		PRODUCT_DEFAULT=$PRODUCT_ONEStore
		echo '你选择了：3、'$PRODUCT_DEFAULT
		;;

		4|5)
		echo '你选择了 4 或 5'
		;;

		*)
		echo '你没有输入 1 到 5 之间的数字'
		;;
	esac
}

# 仍然需要人为选择，不能使用默认，避免将修改好的代码替换了
#PRODUCT_DEFAULT=$2
#if [ ! $PRODUCT_DEFAULT ] ;then
#	echo "没有将产品类型通过参数传递进来"
#	PRODUCT_DEFAULT=$PRODUCT_M2
#	funProductSelect
#else
#	echo "已将产品类型通过参数传递进来，${PRODUCT_DEFAULT}"
#fi
funProductSelect

funProductReadyCompile(){
	echo "copy file "${1}
	touch $COPY_OUT_PATH/${1}
	cp -rf ${1}/* .
	sync
	exit 1
}

if [ "${PRODUCT_DEFAULT}" == "${PRODUCT_M2}" ] ;then
	echo "product type "$PRODUCT_DEFAULT
	if [ -f $COPY_OUT_PATH/$PRODUCT_M2 ];then
		echo "当前的编译环境是M2，不需要切换编译环境"
		exit 0
	fi
	rm $COPY_OUT_PATH/$PRODUCT_M0
	rm $COPY_OUT_PATH/$PRODUCT_ONEStore
	funProductReadyCompile $PRODUCT_M2

elif [ "${PRODUCT_DEFAULT}" == "${PRODUCT_ONEStore}" ] ;then
	echo "product type "$FILE_PATH
	if [ -f $COPY_OUT_PATH/$PRODUCT_ONEStore ];then
		echo "当前的编译环境是ONEStore，不需要切换编译环境"
		exit 0
	fi
	rm $COPY_OUT_PATH/$PRODUCT_M0
	rm $COPY_OUT_PATH/$PRODUCT_M2
	funProductReadyCompile $PRODUCT_ONEStore

elif [ "${PRODUCT_DEFAULT}" == "${PRODUCT_M0}" ] ;then
	echo "product type "$FILE_PATH
	if [ -f $COPY_OUT_PATH/$PRODUCT_M0 ];then
		echo "当前的编译环境是M0，不需要切换编译环境"
		exit 0
	fi
	rm $COPY_OUT_PATH/$PRODUCT_M2
	rm $COPY_OUT_PATH/$PRODUCT_ONEStore
	funProductReadyCompile $PRODUCT_M0

else
	echo "产品类型是错误的"
	exit 0
fi

# 退出值：
# 0表示不切换当前的编译产品类型
# 1表示切换当前的编译产品类型