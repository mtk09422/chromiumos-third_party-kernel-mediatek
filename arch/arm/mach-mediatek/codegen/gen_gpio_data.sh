#!/bin/sh

if [ $# -ne 4 ]; then
	echo "Usage:"
	echo "   ./gen_gpio_data.sh <Txt file> <Chip> <Min field> <Hdr row num>"
	echo "    <Txt file>: generate from gpio pinmux table, etc."
	echo "    <Chip>: should be like mt8135, mt8127, etc."
	echo "    <Min field>: means how many columns in a line."
	echo "    <Hdr row num>: the line number of the header."
	echo ""
	echo "    Example:" 
	echo "		./gen_gpio_data.sh MT8135_GPIO_20130423_page1.txt mt8135 34 5"
	echo "		./gen_gpio_data.sh MT8173_GPIO_20140417.txt mt8173 15 4"
	echo ""
	exit 0
fi

OUT_PATH=`pwd`
OUT_FILE1=pinctrl-mtk-$2.h
OUT_FILE2=$2-pinfunc.h
./soc_gen_data.py $1 $2 $3 $4 -c > $OUT_PATH/$OUT_FILE1
./soc_gen_data.py $1 $2 $3 $4 -t > $OUT_PATH/$OUT_FILE2

