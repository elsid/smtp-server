SHELL = bash

PDFLATEX = pdflatex -interaction=nonstopmode -shell-escape

TEX = $(wildcard tex/*.tex)
SVG = $(wildcard img/*.svg)
SVG_PDF = $(patsubst img/%.svg, img/%.pdf, $(SVG))

all: doc.pdf

doc.pdf: $(TEX) $(SVG_PDF)
	$(PDFLATEX) doc.tex &> /dev/null && \
	$(PDFLATEX) doc.tex &> /dev/null || \
	{ rm doc.pdf; exit 1; }

img/%.pdf: img/%.svg
	inkscape -z --export-pdf=$@ --file=$<

clean:
	rm doc.pdf doc.aux doc.log doc.out doc.toc tex/*.log tex/*.aux tex/*.out tex/*.bbl tex/*.blg tex/*.toc $(SVG_PDF)
