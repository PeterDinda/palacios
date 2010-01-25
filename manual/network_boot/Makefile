LATEX2E = latex
BIBTEX = bibtex
DVIPS = dvips
XDVI = xdvi
PS2PDF = ps2pdf -dMaxSubsetPct=100 -dCompatibilityLevel=1.3 -dSubsetFonts=true -dEmbedAllFonts=true -dPDFSETTINGS=/prepress

TEXDOCS = Network_Booting_Kitten_using_PXE.tex 
WORDDOCS= 
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

clean:
	-rm -f *.dvi *.pdf $(ERASEABLEPSDOCS)
