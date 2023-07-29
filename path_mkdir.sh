#!/bin/bash

SRC_FILE_PATH=$2
FILE_PATH=${SRC_FILE_PATH%/*}
echo "copy file "$SRC_FILE_PATH
echo "copy path "$FILE_PATH

if [ x"$1" == "xM0" ] ;then
	echo "mkdir path "$FILE_PATH
	mkdir -p M0/$FILE_PATH
	
	cp $SRC_FILE_PATH M0/$FILE_PATH
	
	#find M0
fi

if [ x"$1" == "xM2" ] ;then
	echo "mkdir path "$FILE_PATH
	mkdir -p M2/$FILE_PATH
	
	cp $SRC_FILE_PATH M2/$FILE_PATH
	
	#find M2
fi

if [ x"$1" == "xONEStore" ] ;then
	echo "mkdir path "$FILE_PATH
	mkdir -p ONEStore/$FILE_PATH
	
	cp $SRC_FILE_PATH ONEStore/$FILE_PATH
	
	#find ONEStore
fi

# 拷贝语言使用
if [ x"$1" == "xLGG" ] ;then
	SRC_FILE=${SRC_FILE_PATH##*/}
	echo $SRC_FILE

	echo "cp KO file "$SRC_FILE_PATH
	mkdir -p LGG/$FILE_PATH
	cp $SRC_FILE_PATH LGG/$FILE_PATH

	CN_FILE_PATH=${FILE_PATH%/*}/values-zh-rCN
	CN_SRC_FILE=$CN_FILE_PATH/$SRC_FILE
	mkdir -p LGG/$CN_FILE_PATH
	echo "cp CN file "$CN_SRC_FILE
	cp $CN_SRC_FILE LGG/$CN_FILE_PATH

	EN_FILE_PATH=${FILE_PATH%/*}/values
	EN_SRC_FILE=$EN_FILE_PATH/$SRC_FILE
	mkdir -p LGG/$EN_FILE_PATH
	echo "cp CN file "$EN_SRC_FILE
	cp $EN_SRC_FILE LGG/$EN_FILE_PATH

	#find language
fi
