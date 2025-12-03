POWER DISTRIBUTION

(Breadboard Rails)

+----------------+

\[ARDUINO UNO]              | 5V Rail (+)    |

|           |              +----------------+

|        5V +------------->| Connect all VCC|

|       GND +------------->| Connect all GND|

|           |              +----------------+

|           |

|           |              USER INTERFACE (Buttons)

|           |              (One leg to Pin, One leg to GND)

|        D4 +------------->\[ Btn: Menu  ]--------(GND)

|        D5 +------------->\[ Btn: Up    ]--------(GND)

|        D6 +------------->\[ Btn: Down  ]--------(GND)

|        D7 +------------->\[ Btn: Start ]--------(GND)

|           |

|           |              RELAY MODULE (2-Channel)

|        D8 +------------->\[ IN1 ] Relay 1 (Fill Pump)

|        D9 +------------->\[ IN2 ] Relay 2 (Drain Pump)

|           |              \[ VCC ]-----(+5V)

|           |              \[ GND ]-----(GND)

|           |

|           |              ALARM

|       D10 +------------->\[ (+) Buzzer (-) ]----(GND)

|           |

|           |              ULTRASONIC SENSOR (HC-SR04)

|       D11 +------------->\[ Trig ]

|       D12 +------------->\[ Echo ]

|           |              \[ VCC  ]-----(+5V)

|           |              \[ GND  ]-----(GND)

|           |

|           |              LCD DISPLAY (I2C 16x2)

|        A4 +------------->\[ SDA ]

|        A5 +------------->\[ SCL ]

|           |              \[ VCC ]-----(+5V)

|           |              \[ GND ]-----(GND)

+-----------+

