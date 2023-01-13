#!/usr/bin/env python3
# -*- coding: utf-8 -*-


from PyQt5.QtCore import QTimer, pyqtSignal
from PyQt5.QtWidgets import QApplication, QWidget, QErrorMessage, QGridLayout, QTabWidget, QMainWindow, QVBoxLayout
from PyQt5.QtGui import QIcon, QPalette, QTextFormat, QFont

from ui_mainwindow import Ui_MainWindow
from sdm_simulator import SDMSimulator
from warp_charger_simulator import WARPChargerSimulator

from tinkerforge.ip_connection import IPConnection
from tinkerforge.bricklet_industrial_digital_in_4_v2 import BrickletIndustrialDigitalIn4V2
from tinkerforge.bricklet_industrial_quad_relay_v2 import BrickletIndustrialQuadRelayV2

import sys

HOST = "localhost"
PORT = 4223

def a_to_kwh(value, phase=3):
    return 230*phase*value/1000.0

def kwh_to_a(value, phase=3):
    return (value*1000.0)/(230*phase)

def label_round(value):
    return str(round(value, 2))

class WARPEnergyManagerTesterMainWindow(QMainWindow, Ui_MainWindow):
    def __init__(self, parent=None):
        QMainWindow.__init__(self, parent)
        self.setupUi(self)
        self.setWindowTitle('WARP Energy Manager Tester')

        self.phases = 3

        self.sdm_sim = SDMSimulator('W2J')
        #self.wc_sim = WARPChargerSimulator()

        self.slider_main.valueChanged.connect(self.slider_main_value_changed)
        self.slider_house_consumption.valueChanged.connect(self.slider_house_consumption_value_changed)
        self.slider_pv_excess.valueChanged.connect(self.slider_pv_excess_value_changed)
        self.slider_car_consumption.valueChanged.connect(self.slider_car_consumption_value_changed)
        self.checkbox_car_connected.stateChanged.connect(lambda _: self.value_changed())

        self.slider_main_value_changed(self.slider_main.value())
        self.slider_house_consumption_value_changed(self.slider_house_consumption.value())
        self.slider_pv_excess_value_changed(self.slider_pv_excess.value())
        self.slider_car_consumption_value_changed(self.slider_car_consumption.value())

        self.init_tf()


    def init_tf(self):
        self.ipcon = IPConnection()
        self.idi4 = BrickletIndustrialDigitalIn4V2('XQF', self.ipcon)
        self.iqr = BrickletIndustrialQuadRelayV2('ZwY', self.ipcon)

        self.ipcon.connect(HOST, PORT)

        self.tf_timer = QTimer()
        self.tf_timer.timeout.connect(self.tf_loop)
        self.tf_timer.start(100)

    def tf_loop(self):
        self.iqr.set_selected_value(0, self.checkbox_in3.isChecked())
        self.iqr.set_selected_value(1, self.checkbox_in4.isChecked())

        value_in = self.idi4.get_value()
        if value_in[0]:
            self.label_contactor.setText('3-phasig')
            self.phases = 3
        else:
            self.label_contactor.setText('1-phasig')
            self.phases = 1

        if value_in[1]:
            self.label_out.setText('High')
        else:
            self.label_out.setText('Low')

        value_qr = self.iqr.get_value()
        if value_qr[0]:
            self.checkbox_in3.setText('High')
        else:
            self.checkbox_in3.setText('Low')
        if value_qr[1]:
            self.checkbox_in4.setText('High')
        else:
            self.checkbox_in4.setText('Low')

    def get_car_phases(self):
        return self.phases

    def value_changed(self):
        house_consumption_a   = self.slider_house_consumption.value()/1000.0
        pv_excess_a           = self.slider_pv_excess.value()/1000.0
        car_consumption_a     = self.slider_car_consumption.value()/1000.0
        if self.checkbox_car_connected.isChecked():
            self.label_car_consumption_a.setEnabled(True)
            self.label_car_consumption_kwh.setEnabled(True)
        else:
            self.label_car_consumption_a.setEnabled(False)
            self.label_car_consumption_kwh.setEnabled(False)
            car_consumption_a = 0

        if hasattr(self, 'wc_sim'):
            self.wc_sim.set_ma(car_consumption_a*1000)

        house_consumption_kwh = a_to_kwh(house_consumption_a)
        pv_excess_kwh         = a_to_kwh(pv_excess_a)
        car_consumption_kwh   = a_to_kwh(car_consumption_a, self.get_car_phases())

        sum_kwh = house_consumption_kwh + car_consumption_kwh - pv_excess_kwh
        sum_a   = kwh_to_a(sum_kwh)

        if hasattr(self, 'sdm_sim'):
            self.sdm_sim.set_register(53, sum_kwh * 1000)

        self.label_sum_consumption_kwh.setText(label_round(sum_kwh))
        self.label_sum_consumption_a.setText(label_round(sum_a))

    def slider_main_value_changed(self, value):
        a = value/1000.0
        self.label_main_a.setText(label_round(a))
        self.label_main_kwh.setText(label_round(a_to_kwh(a)))

        self.value_changed()

    def slider_house_consumption_value_changed(self, value):
        a = value/1000.0
        self.label_house_consumption_a.setText(label_round(a))
        self.label_house_consumption_kwh.setText(label_round(a_to_kwh(a)))

        self.value_changed()

    def slider_pv_excess_value_changed(self, value):
        a = value/1000.0
        self.label_pv_excess_a.setText(label_round(a))
        self.label_pv_excess_kwh.setText(label_round(a_to_kwh(a)))

        self.value_changed()

    def slider_car_consumption_value_changed(self, value):
        a = value/1000.0
        self.label_car_consumption_a.setText(label_round(a))
        self.label_car_consumption_kwh.setText(label_round(a_to_kwh(a, self.get_car_phases())))

        self.value_changed()

def main():
    args = sys.argv
    application = QApplication(args)
    main_window = WARPEnergyManagerTesterMainWindow()
    main_window.show()
    sys.exit(application.exec_())

if __name__ == "__main__":
    main()
