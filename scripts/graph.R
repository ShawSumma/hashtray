# Graphing for results of the simulation built using libhashtray.
# Nik Sultana, UPenn, January 2018
#
# Usage: R --vanilla  < graph.R
#
# While working on this I benefited from an example given at http://www.r-graph-gallery.com/104-plot-lines-with-error-envelopes-ggplot2/

library(scales)
library(ggplot2)

# FIXME const
path_prefix_With <- "/Users/nik/src/pchasht/with_pchast.data_O1_units_"
path_prefix_Without <- "/Users/nik/src/pchasht/without_pchast.data_O1_units_"
pdf("simulation_1000000hosts.pdf")

uri_prefix <- "file://"

for (PGH in 0:100) {
  file_name_With <- paste(c(path_prefix_With, toString(PGH)), collapse='')
  file_name_Without <- paste(c(path_prefix_Without, toString(PGH)), collapse='')
  if (file.exists(file_name_With) && file.exists(file_name_Without)) {
    #print(paste("Generating graph for PGH =", toString(PGH)))

    data_With <- read.table(paste(c(uri_prefix, file_name_With), collapse='') ,  header=TRUE)
    data_Without <- read.table(paste(c(uri_prefix, file_name_Without), collapse='') , header=TRUE)
    data_With$type <- "Filtered"
    data_Without$type <- "Unfiltered"
    combined_data <- rbind(data_With, data_Without)

    graph <- ggplot(data=combined_data, aes(x=PGC, y=Davg, ymin=Dmin, ymax=Dmax, fill=type, linetype=type)) +
      geom_line() +
      geom_ribbon(alpha=0.5) +
      scale_y_continuous(labels = scales::percent) +
      scale_x_continuous(breaks=c(0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100),
                         labels = c("0%", "10%", "20%", "30%", "40%", "50%", "60%", "70%", "80%", "90%", "100%")) +
      xlab("'Good' Connections") +
      ylab("Stall") +
      theme_bw()

    print(graph + ggtitle(paste(c("Reponse stall at PGH=", toString(PGH)), collapse='')))
  }
}

dev.off()
quit("yes")
