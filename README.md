# Poppler SVG

Add basic support for SVG output to Poppler utils/pdftohtml.  
This is master branch, SVG code is maintained in [svg*](https://github.com/vivo75/poppler/tree/svg) branch of this repository

In 2007 we needed to transform and parse some PDFs. The files were almost only text and a few strokes.
So this patchset was born.

As the careful reader has already guessed aestetic has been put in second place after correctness of text. However the generated file can be opened in inkscape and inspected.

Nowadays better options exists for extracting text from PDFs, for example [pdfalto](https://github.com/kermitt2/pdfalto) which is based on ALTO format. But the generated file cannot be visualized directly.

If you were looking just for Poppler code upstream repository is located [here](https://gitlab.freedesktop.org/poppler/poppler)
