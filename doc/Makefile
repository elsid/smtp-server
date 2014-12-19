SHELL = bash

PDFLATEX = pdflatex -interaction=nonstopmode -shell-escape

TEX = $(wildcard tex/*.tex)
SVG = $(wildcard inc/*.svg)
PDF = $(patsubst inc/%.svg, inc/%.pdf, $(SVG)) inc/fsm.pdf
FSM2DOT = ../utils/fsm2dot.py
DOT2TEX = dot2tex -ftikz --autosize --crop
SVG2PDF = inkscape -z
DOXYGEN = doxygen

all: doc.pdf

doc.pdf: $(TEX) $(PDF) inc/fsm.tex
	$(PDFLATEX) doc.tex &> /dev/null && \
	$(PDFLATEX) doc.tex &> /dev/null

inc/%.pdf: inc/%.svg
	cat $< | \
	sed 's/textLength="[^"]*" *//g' | \
	sed 's/font-size="[^"]*"/font-size="14"/g' | \
	sed 's/lengthAdjust="spacingAndGlyphs" *//g' | \
	sed 's/font-family="[^"]*"/font-family="Times New Roman"/g' > $<.conv
	$(SVG2PDF) --export-pdf=$@ --file=$<.conv

inc/%.dot: ../src/%.def $(FSM2DOT)
	$(FSM2DOT) < $< > $@

inc/%.tex: inc/%.dot
	$(DOT2TEX) < $< > $@

inc/%.pdf: inc/%.tex
	$(PDFLATEX) -output-directory inc $< &> /dev/null

doxygen: doxygen.cfg ../src/*
	$(DOXYGEN) doxygen.cfg

clean:
	rm -rf doc.pdf doc.aux doc.log doc.out doc.toc tex/*.aux $(SVG_PDF) inc/fsm.* inc/*.pdf inc/*.tex tex/*.log inc/*.svg.conv
