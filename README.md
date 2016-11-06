# AutoFillMissingData

A LibreOffice Calc extension that fills missing data (both numerical and categorical) using machine learning techniques.

To install the prebuilt extension, use the Extension Manager of LibreOffice and browse to this repo's file `AutoFillMissingData.oxt`. Alternatively you can install the extension using terminal as :
```
$ git clone https://github.com/dennisfrancis/AutoFillMissingData.git
$ cd AutoFillMissingData
$ unopkg install ./AutoFillMissingData.oxt
```

To use this extension on some data in a sheet in LibreOffice, place the cursor on any cell inside your table with data ( no need to select the whole table ) and go to the menu `Missing data` and click on `Fill missing data`.


This project is currently under heavy development. Full source code is made available under [GPL3 license](https://www.gnu.org/licenses/gpl-3.0.en.html).
If you are interested in understanding how to build LibreOffice extensions like this, I have a blog for that at [https://niocs.github.io/LOBook/extensions/index.html](https://niocs.github.io/LOBook/extensions/index.html)

As of now the extension uses a variation of kNN instance based algorithm to predict missing data. Ability to tune the algorithm parameters via dialogue boxes is coming soon.

A major caveat is that the prebuilt extension (.oxt file) supports only modern GNU/Linux 64 bit systems comparable to Fedora 24. However support for MS Windows and MacOSX is planned.

Watch this space for feature updates !
