LATEX2E = pdflatex
BIBTEX = bibtex
DVIPS = dvips
XDVI = xdvi
PS2PDF = ps2pdf -dMaxSubsetPct=100 -dCompatibilityLevel=1.2 -dSubsetFonts=true -dEmbedAllFonts=true  -dPDFSETTINGS=/prepress

TEXDOCS = manual.tex 
WORDDOCS= 
DVIDOCS = $(TEXDOCS:.tex=.dvi)
ERASEABLEPSDOCS=$(TEXDOCS:.tex=.ps)
PSDOCS  = $(ERASEABLEPSDOCS) $(WORDDOCS:.doc=.ps) 
PDFDOCS = $(PSDOCS:.ps=.pdf)

all: $(PDFDOCS)  

#%.pdf: %.ps
#%	$(PS2PDF) $< $(@F)

#%.ps : %.dvi
#	$(DVIPS) -t letter  -Ppdf -G0 -f $< -o $(@F)

#%.dvi: %.tex *.eps
#	$(LATEX2E) $<

%.pdf: %.tex 
	$(LATEX2E) $<

bib: 
	$(LATEX2E) manual
	$(LATEX2E) manual


clean:
	-rm -f *.dvi manual.pdf $(ERASEABLEPSDOCS)
