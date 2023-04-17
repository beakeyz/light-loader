The LightLoader disk driver model

This markdown file is meant to give a brief overview of the disk device model that lightloader uses.

Overview:
  - Layout
  - Function
  - Effectiveness

# Layout

The way this layout is designed is with inverse abstraction in mind. In the root of the drivers source tree we have the different kinds of abstractions that we may deal with in this driver. As we progress into the source tree however, we find more and more abstraction.

    root           n levels into 
                   the sourcetree
    ___________________________________
    Disk ----\
              \
    Part ------|---- Filesystem
              /
    Volume --/

# Function

TODO

# Effectiveness

TODO
