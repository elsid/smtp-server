SHELL = bash

PDFLATEX = pdflatex -interaction=nonstopmode -shell-escape
FSM2DOT = utils/fsm2dot.py
DOT2TEX = dot2tex -ftikz --autosize --crop
SVG2PDF = inkscape -z

TEX = $(wildcard tex/*.tex)
SVG = $(wildcard inc/*.svg)
DEF = $(wildcard ../src/*.def)

PDF += inc/title.pdf
PDF += $(patsubst inc/%.svg, inc/%.pdf, $(SVG))
PDF += $(patsubst ../src/%.def, inc/%.pdf, $(DEF))

LOG += ../var/log/test_module_result.log
LOG += ../var/log/test_system_result.log
LOG += ../var/log/test_memory_result.log

MAKEFILE += Makefile
MAKEFILE += ../Makefile

CFG += ../etc/smtp-server.cfg
CFG += ../etc/test_system.cfg
CFG += ../etc/test_memory.cfg

all: doc.pdf

doc.pdf: $(TEX) $(PDF) $(MAKEFILE) $(LOG) $(CFG)
	$(PDFLATEX) doc.tex &> /dev/null && \
	$(PDFLATEX) doc.tex &> /dev/null

inc/%.pdf: inc/%.conv.svg
	$(SVG2PDF) --export-pdf=$@ --file=$<

inc/%.conv.svg: inc/%.svg
	cat $< | \
	sed 's/textLength="[^"]*" *//g' | \
	sed 's/font-size="[^"]*"/font-size="14"/g' | \
	sed 's/lengthAdjust="spacingAndGlyphs" *//g' | \
	sed 's/font-family="[^"]*"/font-family="Times New Roman"/g' > $@

inc/%.svg: inc/%.dot
	dot -Tsvg $< > $@

inc/%.dot: ../src/%.def $(FSM2DOT)
	$(FSM2DOT) < $< > $@

clean:
	rm -rf $(PDF) doc.pdf doc.aux doc.log doc.out doc.toc doc.idx tex/*.aux \
		inc/fsm.* inc/*.pdf inc/*.tex tex/*.log inc/*.svg.conv
