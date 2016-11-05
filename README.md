# AutoFillMissingData

A LibreOffice Calc spreadsheet extension that fills missing data using machine learning techniques.

To install the prebuilt extension, use the Extension Manager of LibreOffice and browse to this repo's file `AutoFillMissingData.oxt`. Alternatively you can install the extension using terminal as :
```
$ git clone https://github.com/dennisfrancis/AutoFillMissingData.git
$ cd AutoFillMissingData
$ unopkg install ./AutoFillMissingData.oxt
```

To use this extension on some data in a sheet in LibreOffice, place the cursor on any cell inside your table with data ( no need to select the whole table ) and go to the menu `Missing data` and click on `Fill missing data`.


This project is currently under heavy development. Full source code is available under [GPL3 license](https://www.gnu.org/licenses/gpl-3.0.en.html).
If you are interested in understanding how to build LibreOffice extension, I have a blog for that at [https://niocs.github.io/LOBook/extensions/index.html](https://niocs.github.io/LOBook/extensions/index.html)

As of now it just uses data imputing to fill the blank cells, but much of the code is in place for doing predictive analytics which would then fill the blank cells instead.
Another caveat is that the extension now supports only modern GNU/Linux 64 bit systems. However support for MS Windows and MacOSX is planned.
Watch this space for feature updates !
