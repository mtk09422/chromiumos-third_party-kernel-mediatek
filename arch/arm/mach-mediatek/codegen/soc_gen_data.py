#!/usr/bin/python
# Copyright (c) 2014 MediaTek Inc.
# Author: Hongzhou.Yang <hongzhou.yang@mediatek.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

#
# Usage:
#    soc_gen_data.py <GPIO Table Filaname> <SOC name> <min field> <hdr row num>


import sys
import re
import string

# offset_A = 0x10005000
# offset_B = 0x1020C000


def load_data(fn, ncol):
    data =[]
    cont = False
    buf = ''
    with open(fn) as f:
        for line in f:
            if line[0] == '#':
                continue

            buf += line
            a = buf[0:-1].split('\t')
            if len(a) < ncol:
                continue
            buf = ''
            for n, elem in enumerate(a):
                if len(elem) == 0:
                    a[n] = None

            if ncol > 0:
                data.append(a[0:ncol])
            else:
                data.append(a)
    return data


def load_groups(fn):
    data = load_data(fn, 4, '\t\n')
    groups ={}
    for item in data:
        name = item[0].upper()
        reg = int(item[2], 16)
        shift = int(item[3], 10)
        if groups.get(name) != None:
            print >> sys.stderr, "Duplicate group definition: %s; ignoring" % name
            continue
        groups[name] =(name, reg, shift)
    return groups


def load_pins(fn, grps):
    data = load_data(fn, 11, '\t\n')
    pins = []
    for item in data:
        n = int(item[0], 10)
        name = item[1]
        if name is not None:
            name = name.upper()
            if name == "N/A":
                name = None

        grp = grps.get(name)
        if grp is None and name != None:
            print "pin %d references undefined group %s; group ignored" % (n, name)
        modes = []
        for mode in item[2:10]:
            if mode is None:
                modes.append("NULL")
            else:
                modes.append('"%s"' % mode)
        pins.append((n, grp, (modes)))
    return pins


def gen_group_enum(grps, enum_name, pfx):
    grp_list = []
    for (name, reg ,shf) in grps.values():
        grp_list.append((name, reg, shf))
    grp_list.sort(key= lambda item: item[0])
    print "enum %s {" % enum_name
    print "\t%sNONE = -1," % pfx
    for item in grp_list:
        print "\t%s%s," % (pfx, item[0])
    print "};"


def gen_group_table(grps, tbl_type, tbl_name):
    print "struct %s %s[] = {" % (tbl_type, tbl_name)
    for grp in grps.values():
        print grp
    print "};"


def gen_pin_table(pins, tbl_type, tbl_name, grp_pfx):
    print "struct %s %s[] = {" % (tbl_type, tbl_name)
    for pin in pins:
        grp = pin[1]
        grp_name = grp[0] if grp is not None else "NONE"
        modes =pin[2]
        print "\t{ .id = %d, .group_id = %s%s," % (pin[0], grp_pfx, grp_name)
        print "\t\t.modes = { %s," % ", ".join(modes[0:4])
        print "\t\t\t%s }, }," % ", ".join(modes[4:8])
    print "};"


def process_my_tables(grp_file, pin_file):
    grps = load_groups(grp_file)
    grp_list = sort_groups(grps)
    grp_pfx = "MT_PINGRP_"
    gen_group_enum(grps, "MT_PIN_GROUPS", grp_pfx)
    gen_group_table(grps, "mt_pin_group_data", "mt_pin_group_data")
    pins = load_pins(pin_file, grps)
    gen_pin_table(pins, "mt_pin_data", "mt_pin_data", grp_pfx)


class GpioParamType(object):
    def __init__(self, idx, name, enabled, repeated):
        self.idx = idx
        self.name = ''
        # if we want to see this in our output
        self.enabled = enabled
        # if we want co get default value from previous row
        self.repeated = repeated
        if type(name) == str:
            for n in name:
                if n in '\r\n\t':
                    n = ' '

                self.name += n

    def __str__(self):
        return "{idx=%d,name=%s,enabled=%d,repeated=%d}" % (self.idx, self.name, self.enabled ,self.repeated)


class GpioPin(object):
    def __init__(self):
        pass


class GpioFunctionValue(object):
    def __init__(self,  muxval,  funcname):
        self.muxval = muxval
        self.func = funcname

class GpioParamValue(object):
    def __init__(self, typ, val):
        self.typ = typ
        self.val = val

    def __str__(self):
        return "{type=%s,value=%s}" % (self.typ, self.val)

    def print_val(self, indent=2):
        if self.val is not None:
            a = self.val.split('\n')
            multi = len(a) > 1
            if multi:
                print " "*(indent-1), "{"
                indent += 2
            for e in a:
                if indent > 0:
                    print " "*(indent-1), e
                else:
                    print e
            if multi:
                indent -= 2
                print " "*(indent-1), "}"
        else:
            print 'None'


def gen_mt_pin_def_tbl(gpio_files, chip_id):
    print '#ifndef __PINCTRL_MTK_%s_H'  % (
        chip_id.upper()
    )
    print '#define __PINCTRL_MTK_%s_H\n'  % (
        chip_id.upper()
    )
    print '#include <linux/pinctrl/pinctrl.h>'
    print '#include <pinctrl-mtk-common.h>\n'
    print 'static const struct mtk_desc_pin mtk_pins_%s[] = {' % (
        chip_id
    )
#    for f in gpio_files:
    gpio_files.gen_mt_pin_def_tbl_body()
    print '};\n'

def gen_mt_gpio_dts_def(gpio_files,  chip_id):
    print '#ifndef __DTS_%s_PINFUNC_H'  % (
        chip_id.upper()
    )
    print '#define __DTS_%s_PINFUNC_H\n'  % (
        chip_id.upper()
    )
    print '#include <dt-bindings/pinctrl/mt65xx.h>\n'
    gpio_files.gen_mt_gpio_dts_def_body(chip_id)


class MtkGpioFile(object):
    def __init__(self, enable_list, repeat_list, chip, drv_class_info):
        self.field_map = {}  # do name -> idx translation
        self.field_tbl = []  # do idx -> name translation
        self.gpios =[]
        self.chip = chip
        self.enabled = enable_list
        self.repeated = repeat_list
        self.drv_class_info = drv_class_info
        self.no_grp = ("NA", "CSI_", "DSI_")

    def get_gpio_drv_grp(self, gpio):
        drv_grp_name = self.get_gpio_val(gpio, 'DRV')
        if drv_grp_name:
            m = re.match('(.+)(_conf)(\d+)?\[\d:\d\]', drv_grp_name)
            if m is None:
                m = re.match('(.+)(gpi_drive_en)\[\d:\d\]', drv_grp_name)

            if m is not None:
                ml = [e if e is not None else "" for e in m.groups()]
                ml[1] = ''
                if len(ml) > 2 and ml[2] != '':
                    ml[1] = '_'

                drv_grp_name = "".join(ml)
            drv_grp_name = drv_grp_name.upper()
        if drv_grp_name in self.no_grp:
            drv_grp_name = None
        return drv_grp_name

    def get_gpio_pin_name(self, gpio):
        pin_name = self.get_gpio_val(gpio, 'Pin Name').upper()
        if pin_name.startswith('PAD_'):
            pin_name = pin_name[4:]
        return pin_name

    def load_hdr(self, hdr):
        prev = ''
        cnt = 1
        for i, field in enumerate(hdr):
            if field is None:
                cnt += 1
                field = '%s%d' % (prev, cnt)
            else:
                cnt = 1
                prev = field
            item = GpioParamType(i, field, field in self.enabled, field in self.repeated)
            self.field_map[field] = item
            self.field_tbl.append(item)

    def add_gpio(self, line, data):
        gpio = []
        valid_cnt = 0
        prev = self.gpios[-1] if len(self.gpios) else None
        for i in xrange(len(self.field_tbl)):
            typ = self.field_tbl[i]
            val = data[i]
            if val == 'x':
                val = None
            if val is not None:
                valid_cnt += 1
            elif prev and typ.repeated:
                val = prev[i].val
            gpio.append(GpioParamValue(typ, val))
        if valid_cnt > 0:
            self.gpios.append(gpio)
        else:
            print >> sys.stderr, 'chip=%s: empty record at line %d; ignored' % (self.chip, line)

    def print_tbl(self):
        for i, gpio in enumerate(self.gpios):
            print "REC#%d" % i
            for prop in gpio:
                if prop.typ.enabled:
                    print "%s:" % prop.typ.name,
                    prop.print_val()

    def get_gpio_val(self, gpio, prop_name):
        f_prop = self.field_map.get(prop_name)
        return gpio[f_prop.idx].val if f_prop is not None else None

    def gen_gpio_defs(self):
        f_pin_name = self.field_map['Pin Name']
        f_pad_name = self.field_map['Pin Num']
        f_drv = self.field_map['DRV']
        f_drv2 = self.field_map['DRV2']
        f_tdsel = self.field_map['TDSEL']
        f_rdsel = self.field_map['RDSEL']
        f_pu_pd = self.field_map['PU/PD']
        f_pull_en = self.field_map['PullEn']
        f_pull_sel = self.field_map['PullSel']
        f_mode = self.field_map['Mode']
        f_mode_reg = self.field_map['Mode Register']
        f_eint_num = self.field_map['EINT Num']
        f_aux = [func for func in self.field_tbl if func.name.startswith('Aux Func')]

        mode_reg_map = {}
        drv_reg_map = {}
        tdsel_reg_map = {}
        rdsel_reg_map = {}

        print 'struct mt_pin_desc mt_pin_desc_tbl[] = {'
        for gpio in self.gpios:
            modes = [gpio[f.idx].val for f in f_aux]
            eint_mode = -1
            gpio_mode = -1
            eint_num = -1
            gpio_num = -1
            for i in xrange(len(modes)):
                mode = modes[i]
                if mode is not None:
                    mode = modes[i].split(':')[-1]
                    if mode.startswith("GPI") or mode.startswith("GPO"):
                        gpio_mode = i
                        if mode.startswith("GPIO"):
                            gpio_num = int(mode[4:], 10)
                        else:
                            gpio_num = int(mode[3:], 10)
                    if mode.startswith("EINT"):
                        eint_mode = i
                        eint_num = int(mode[4:], 10)
                modes[i] = quoted_str(mode)
            print '{ .id=%d, .eint=%d, .pin=%s, .pad=%s, .mode_reg=%s,' % (
                gpio_num,
                eint_num,
                quoted_str(pin_name),
                quoted_str(self.get_gpio_val(gpio, 'Pin Num')),
                quoted_str(self.get_gpio_val(gpio, 'Mode Register')),
            )
            print '  .drv_reg=%s, .tdsel_reg=%s, .rdsel_reg=%s,' % (
                quoted_str(self.get_gpio_val(gpio, 'DRV')),
                quoted_str(self.get_gpio_val(gpio, 'TDSEL')),
                quoted_str(self.get_gpio_val(gpio, 'RDSEL')),
            )
            print '  .modes = { %s }, },' % (
                ", ".join(modes)
                )
        print '};'

    def find_gpio_class(self, pin):
        probed_values = pin.drv_values
        for cls_name, cls_values in self.drv_class_info.items():
            matches = reduce(lambda cnt, val: cnt+1 if val in cls_values and val != 0 else cnt, probed_values, 0)
            valid = reduce(lambda cnt, val: cnt+1 if val != 0 else cnt, cls_values, 0)
            if valid == matches:
                return cls_name

        return None

    def parse_mt_pin_def_tbl(self):
        f_aux = [func for func in self.field_tbl if func.name.startswith('Aux Func')]

        pins =[None for i in xrange(len(self.gpios))]

        for gpio in self.gpios:
            pin = GpioPin()
            modes = [gpio[f.idx].val for f in f_aux]
            pin_name = self.get_gpio_val(gpio, 'Pin Name')
            pad_name = self.get_gpio_val(gpio, 'Pin Num')
            pin_name = pin_name.upper()
            if pin_name.startswith('PAD_'):
                pin_name = pin_name[4:]

            if pad_name:
                pad_name = pad_name.upper()

            pin.pin_name = pin_name
            pin.pad_name = pad_name
            if len(filter(lambda x: x is not None, modes)) == 0:
                print >> sys.stderr, "chip=%s, pin=%s, pad=%s: no pinmux modes; ignored" % (self.chip, pin_name, pad_name)
                continue
            for i in xrange(len(modes)):
                mode = modes[i]
                if mode is not None:
                    mode = modes[i].split(':')[-1].upper()
                modes[i] = quoted_str(mode)
            pin.modes = modes
            # GPIO mode must exist
            gpmode = filter(lambda x: x.startswith('"GPIO'), pin.modes)
            if len(gpmode) > 0:
                pin.pin_num = int(gpmode[0][5:-1])
            else:
                gpmode = filter(lambda x: x.startswith('"GPI'), pin.modes)
                pin.pin_num = int(gpmode[0][4:-1])
            pin.chip = self.chip

            pin.eint_num = -1
            eint_num = self.get_gpio_val(gpio, 'EINT Num')
            if eint_num and eint_num.isdigit():
                pin.eint_num = int(eint_num);

            pin.drv_grp_name = self.get_gpio_drv_grp(gpio)
            if pin.drv_grp_name is not None:
                drv_cls_info = self.get_gpio_val(gpio, 'DRV2')
                if len(drv_cls_info) > 2 and drv_cls_info[0] == drv_cls_info[-1] and drv_cls_info[0] == '"':
                    drv_cls_info = drv_cls_info[1:-1]
                drv_items = drv_cls_info.split()
                items = filter(lambda x: not (x.startswith('E') or x.startswith("3'")), drv_items)
                if len(items) == 1:
                    probed_values = items[0][:-2].split('/')
                else:
                    probed_values = []
                    for item in items:
                        probed_values.append(item[:-2])
                pin.drv_values = tuple(int(x) for x in probed_values)

            if pin.chip.upper() == 'MT8135':
                if pin.pin_num >= 34 and pin.pin_num <= 148:
                    pin.pin_type = 1
                else:
                    pin.pin_type = 0
            if pins[pin.pin_num] is None:
                pins[pin.pin_num] = pin
            else:
                print >> sys.stderr, "%s:GPIO%d: duplicated description" % (pin.chip, pin.pin_num)
        self.pins = pins
        for n, pin in enumerate(self.pins):
            if pin is None:
                print >> sys.stderr, "%s:GPIO%d: description is missing" % (self.chip, n)

    def gen_mt_pin_def_tbl_body(self):
        self.parse_mt_pin_def_tbl()
        pin_num = 0
        for pin in self.pins:
            if pin is None:
                continue

            print '\tMTK_PIN('
            print '\t\tPINCTRL_PIN(%d, %s),' % (
                pin_num,
                quoted_str(pin.pin_name),
            )

            print '\t\t%s, %s,' % (
                quoted_str(pin.pad_name),
                quoted_str(self.chip),
            )

            has_eint = 0;
            for mode in range(8):
                if pin.modes[mode].find("NULL", 0, (len("NULL") + 1)) != -1:
                    continue

                if mode==0 and pin.eint_num >= 0:
                    has_eint = 1;
                    print '\t\tMTK_EINT_FUNCTION(%d, %d),' % (
                        mode,
                        pin.eint_num
                    )
                elif pin.modes[mode].find("EINT", 0, (len("EINT") + 1)) != -1:
                    has_eint = 1;
                    mode_str = pin.modes[mode].strip('"')
                    mode_str = mode_str.strip()
                    index = mode_str.find("_", 0, len(mode_str))
                    if index != -1:
                        print '\t\tMTK_EINT_FUNCTION(%d, %d),' % (
                            mode,
                            int(mode_str[4:index])
                        )
                    else:
                        print '\t\tMTK_EINT_FUNCTION(%d, %d),' % (
                            mode,
                            int(mode_str[4:])
                        )

            if has_eint != 1:
                print '\t\tMTK_EINT_FUNCTION(NO_EINT_SUPPORT, NO_EINT_SUPPORT),'

            mode_val_name = []

            for mode in range(8):
                if pin.modes[mode].find("NULL", 0, (len("NULL") + 1)) != -1:
                    continue

                mode_val_name.append(GpioFunctionValue(mode,  pin.modes[mode]))

            vaild_len = len(mode_val_name)
            for i, val_name in enumerate(mode_val_name):
                if i != (vaild_len - 1):
                    print '\t\tMTK_FUNCTION(%d, %s),' %  (
                        val_name.muxval,
                        val_name.func
                    )
                else:
                    print '\t\tMTK_FUNCTION(%d, %s)' %  (
                        val_name.muxval,
                        val_name.func
                    )

            print '\t),'
            pin_num += 1

    def gen_mt_gpio_dts_def_body(self,  chip_id):
        self.parse_mt_pin_def_tbl()
        pin_num = 0
        for pin in self.pins:
            if pin is None:
                continue

            index_slash = pin.pin_name.find("/", 0, len(pin.pin_name))
            index_parhes = pin.pin_name.find("(", 0, len(pin.pin_name))
            if  index_slash != -1:
                pin_string = '#define %s_PIN_%d_%s' % (
                    chip_id.upper(),
                    pin_num,
                    pin.pin_name[0:index_slash],
                )
            elif index_parhes != -1:
                pin_string = '#define %s_PIN_%d_%s' % (
                    chip_id.upper(),
                    pin_num,
                    pin.pin_name[0:index_parhes],
                )
            else:
                pin_string = '#define %s_PIN_%d_%s' % (
                    chip_id.upper(),
                    pin_num,
                    pin.pin_name,
                )

            for mode in range(8):
                if pin.modes[mode].find("NULL", 0, (len("NULL") + 1)) != -1:
                   continue

                mode_str = pin.modes[mode].strip('"')
                mode_str = mode_str.strip()
                index_l = mode_str.find("[", 0, len(mode_str))
                index_r = mode_str.find("]", 0, len(mode_str))
                if index_l !=-1:
                    print '%s__FUNC_%s_%s (MTK_PIN_NO(%d) | %d)' %  (
                            pin_string,
                            mode_str[0:index_l],
                            mode_str[index_l+1: index_r],
                            pin_num,
                            mode,
                    )
                else:
                    print '%s__FUNC_%s (MTK_PIN_NO(%d) | %d)' %  (
                            pin_string,
                            mode_str[0:],
                            pin_num,
                            mode,
                    )
            print ''
            pin_num += 1

def quoted_str(s):
    return '"%s"' % s if s is not None else 'NULL'


def process_mt8135_table(fn, chip, min_field, hdr):
    data = load_data(fn, min_field)
    drv_class_info = {
        # cls_min_max_step
        'cls_2_16_8': (2, 4,  6,  8, 10, 12, 14, 16),
        'cls_4_16_4': (4, 0,  8,  0, 12,  0, 16,  0),
        'cls_2_8_4':  (2, 4,  6,  8,  0,  0,  0,  0),
        'cls_4_32_8': (4, 8, 12, 16, 20, 24, 28, 32),
    }

    gpios = MtkGpioFile(['Pin Num', 'Pin Name',
                        'Aux Func.0', 'Aux Func.1', 'Aux Func.2', 'Aux Func.3',
                        'Aux Func.4', 'Aux Func.5', 'Aux Func.6', 'Aux Func.7',
                        'PU/PD', 'PullEn', 'PullSel', 'Mode', 'Mode Register',
                        'DRV', 'DRV2', 'TDSEL', 'RDSEL', 'EINT Num'],
                        ['Mode Register'], chip, drv_class_info)
    for n, item in enumerate(data):
        if n >= hdr:
            gpios.add_gpio(n, item)
        elif n == (hdr - 1):
            gpios.load_hdr(item)
    return gpios

def process_mt6397_table(fn, chip):
    data = load_data(fn, 22)
    drv_class_info = {
        'cls_2_16_16': (2, 4, 6, 8, 10, 12, 14, 16),
        'cls_2_8_16':  (2, 4, 6, 8),
        'cls_4_32_32': (4, 8, 12, 16, 20, 24, 28, 32),
        'cls_4_16_32': (4, 8, 12, 16),
    }
    gpios = MtkGpioFile(['IO PWR Domain', 'Pin Num', 'Driving', 'Pin Name',
                        'Aux Func.0', 'Aux Func.1', 'Aux Func.2', 'Aux Func.3',
                        'Aux Func.4', 'Aux Func.5', 'Aux Func.6', 'Aux Func.7'],
                        [], chip, drv_class_info)
    for n,item in enumerate(data):
        if n >= 4:
            gpios.add_gpio(n, item)
        elif n == 1:
            h1 = item[-8:]
        elif n == 3:
            hdr = item[0:-8]
            hdr.extend(h1)
            gpios.load_hdr(hdr)
    return gpios

def output_pinctrl_desc_data(chip_id):
    print 'static const struct mt_pinctrl_desc %s_pinctrl_data = {' % (
        chip_id
    )
    print '\t.pins = mt_pins_%s,' % (
        chip_id
    )
    print '\t.npins = ARRAY_SIZE(mt_pins_%s),' % (
        chip_id
    )
    print '};\n'

def main(argv):
    if cmp(argv[2],  "mt6397") == 0:
        chip_datas = process_mt6397_table(argv[1], argv[2])
    else:
        chip_datas = process_mt8135_table(argv[1], argv[2], int(argv[3]), int(argv[4]))

    if cmp(argv[5],  "-c") == 0:
        gen_mt_pin_def_tbl(chip_datas, argv[2])
        print "#endif /* __PINCTRL_MTK_%s_H */" % (
            argv[2].upper()
        )
    elif cmp(argv[5],  "-t") == 0:
        gen_mt_gpio_dts_def(chip_datas, argv[2])
        print "#endif /* __DTS_%s_PINFUNC_H */" % (
            argv[2].upper()
        )

    return 0 if chip_datas is not None else 1

if __name__ == '__main__':
    sys.exit(main(sys.argv))

#main(sys.argv)
