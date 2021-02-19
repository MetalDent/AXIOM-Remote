# Change to the path of XC32 installation, default is /opt/microchip/xc32/<version>/
XC32_PATH=/opt/microchip/xc32/v2.40
CC=${XC32_PATH}/bin/xc32-g++
LD=${XC32_PATH}/bin/xc32-ld

NAME=AXIOM_Remote_Firmware

DEVICE=32MZ2048ECG100

LINKER_SCRIPT=procdefs_app.ld

BUILD_DIR=build
INC_DIR=-I${XC32_PATH}/pic32mx/include -I../Common -I../Bootloader
VPATH=../

# Bootloader modules are added temporarily, till UA can be flashed through BL
SOURCE_FILES=Main.cpp Bootloader/Periphery/UART/UART.cpp Bootloader/Periphery/USB/USBCDCDevice.cpp
#$(wildcard *.cpp) $(wildcard **/*.cpp) $(wildcard UI/**/*.cpp) $(wildcard ../Bootloader/Periphery/**/*.cpp)
OBJ_FILES=${SOURCE_FILES:%.cpp=${BUILD_DIR}/%.o}

OPTS=-c -fstack-usage -Wstack-usage=96 -fdump-tree-optimized -fno-rtti -mprocessor=${DEVICE} -mconfig-data-dir=${XC32_PATH}/pic32mx/lib/proc/${DEVICE} -iprefix ${XC32_PATH}/lib/gcc/pic32mx/4.8.3/ -Wall -MMD
LD_OPTS= -Wl,--defsym=_min_heap_size=128,--trace,--script="${LINKER_SCRIPT}",--cref,-Map="${BUILD_DIR}/${NAME}.map",--report-mem,--warn-section-align,--memorysummary=${BUILD_DIR}/memoryfile.xml

all: ${BUILD_DIR}/${NAME}.elf

${BUILD_DIR}/${NAME}.elf: ${OBJ_FILES}
	@ ${CC} -mprocessor=32MZ2048ECG100 ${LD_OPTS} $^ -o $@
	@ ${XC32_PATH}/bin/xc32-bin2hex $@

${BUILD_DIR}/%.o: %.cpp
	@ mkdir -p $(dir $@)
	${CC} ${OPTS} -MF ${BUILD_DIR}/.depend_$(@F) $^ -o $@ ${INC_DIR}
	@ echo '*---------------*'

.PHONY: clean
clean:
	@ rm -rf ${BUILD_DIR}/*
