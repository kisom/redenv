redenv
======

RedBoard enviromental sensor - so named because it was based on the
`Redboard Turbo`_ from SparkFun.

Current hardware:

+ SparkFun `Thing Plus`_
+ Adafruit `Adalogger`_
+ Adafruit RFM95 900MHz `Radiowing`_
+ SparkFun BME280/CCS811 Qwiic `environmental sensor`_
+ SparkFun `Sunny Buddy`_
+ SparkFun 2.5W `large solar cell`_
+ 2000mAh LiPo battery

Home page: https://hackaday.io/project/168220-redenv

Current code running is in ``node-arduino``: there's no PlatformIO
definition for the Thing Plus SAMD51, and I wanted to eliminate board
differences while debugging some RF issues.

.. _gateway: https://www.sparkfun.com/products/15336
.. _Redboard Turbo: https://www.sparkfun.com/products/14812
.. _Thing Plus: https://www.sparkfun.com/products/14713
.. _Sunny Buddy: https://www.sparkfun.com/products/12885
.. _large solar cell: https://www.sparkfun.com/products/retired/7840
.. _Adalogger: https://www.adafruit.com/product/2922
.. _Radiowing: https://www.adafruit.com/product/3231
.. _environmental sensor: https://www.sparkfun.com/products/14348

