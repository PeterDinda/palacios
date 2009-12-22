LATEX2E = latex
BIBTEX = bibtex
DVIPS = dvips
XDVI = xdvi
PS2PDF = ps2pdf -dMaxSubsetPct=100 -dCompatibilityLevel=1.3 -dSubsetFonts=true -dEmbedAllFonts=true -dPDFSETTINGS=/prepress

TEXDOCS = Palacios_Guest_Build.tex 
DVIDOCS = $(TEXDOCS:.tex=.dvi)
ERASEABLEPSDOCS=$(TEXDOCS:.tex=.ps)
PSDOCS  = $(ERASEABLEPSDOCS) $(WORDDOCS:.doc=.ps) 
PDFDOCS = $(PSDOCS:.ps=.pdf)

all: $(PDFDOCS)  $(PSDOCS) $(DVIDOCS)

%.pdf: %.ps
	$(PS2PDF) $< $(@F)

%.ps : %.dvi
	$(DVIPS) -t letter -f $< -o $(@F)

%.dvi: %.tex
	$(LATEX2E) $<


bib: 
	-$(LATEX2E) Palacios_Guest_Build
	-$(LATEX2E) Palacios_Guest_Build
	-$(BIBTEX)  Palacios_Guest_Build
	-$(LATEX2E) Palacios_Guest_Build
	-$(LATEX2E) Palacios_Guest_Build


clean:
	-rm -f *.dvi *.pdf $(ERASEABLEPSDOCS)
