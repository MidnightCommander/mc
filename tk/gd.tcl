#
# gd: the built in Midnight Commander GUI designer
# (C) 1996 the Free Software Foundation
# See the file COPYING for details
#
# Author: Miguel de Icaza
#

set min_width  10
set min_height 10
set dragging 0
set new_dialog 1

proc reset_counters {} {
    global dialog_rows
    global dialog_columns
    global frame_count
    global text_count 
    global line_count

    set dialog_rows 4
    set dialog_columns 4
    set frame_count 0
    set text_count 0
    set line_count 0
}

#
# create a division
# 
# what = { row, column }
# if visible then allow the user to add columns and make them visibles
#
proc create_division {root index what visible} {
    global dialog_columns
    global dialog_rows

    set cn [expr $index*2]

    if {$what == "row"} { 
	set owhat "column"
	set width height
	set stick we
    } else { 
	set owhat "row"
	set width width 
	set stick ns
    }
    set c \$dialog_${owhat}s

    if {$visible} {
	frame $root.$what@$cn -$width 3  -back gray -relief sunken -borderwidth 4
	bind $root.$what@$cn <Enter> "$root.$what@$cn configure -back red"
	bind $root.$what@$cn <Leave> "$root.$what@$cn configure -back gray"
	bind $root.$what@$cn <ButtonRelease-1> "new_division $root $index $what"
    } else {
	frame $root.$what@$cn -$width 3
    }
    grid $root.$what@$cn -$what $cn -$owhat 0 -${what}span 1 -${owhat}span [expr $c*2] -sticky $stick
}

proc create_column {root column visible} {
    create_division $root $column column $visible
}

proc create_row {root row visible} {
    create_division $root $row row $visible
}

proc column_space {root column} {
    global min_width

    grid columnconfigure $root [expr $column*2+1] -minsize $min_width
}

proc row_space {root row} {
    global min_height

    grid rowconfigure $root [expr $row*2+1] -minsize $min_height
}

#
# When inserting a column or row, move all of the widgets after
# the insertion point
#
proc move_childs {root index what} {
    global components

    set pix [expr $index*2]
    
    foreach i $components {
	set info [grid info $root.$i]
	set idx [lsearch $info -$what]
	if {$idx >= 0} {
	    incr idx
	    set cp [lindex $info $idx]
	    if {$cp >= $pix} {
		grid $root.$i -$what [expr $cp+2]
	    }
	}
    }
}

#
# Update the separators spans after a column or row has been added
#
proc reconfig_spans {root} {
    global dialog_rows
    global dialog_columns
 
    for {set i 0} {$i <= $dialog_rows} {incr i} {
	set j [expr $i*2]
	grid $root.row@$j -columnspan [expr $dialog_columns*2]
    }
    for {set i 0} {$i <= $dialog_columns} {incr i} {
	set j [expr $i*2]
	grid $root.column@$j -rowspan [expr $dialog_rows*2]
    }
}

proc new_division {root index what} {
    global dialog_columns
    global dialog_rows

    set var [incr dialog_${what}s]

    create_$what $root $var 1
    ${what}_space $root $var
    reconfig_spans $root
    move_childs $root $index $what
}

proc create_gui_canvas {frame} {
    if {$frame == "."} { set base "" } else { set base $frame }
    set bw $base.widgets
    catch "frame $bw"
    grid   $bw -column 1 -row 1 -sticky nwse -padx 2 -pady 2 -ipady 12
}

proc create_workspace {frame} {
    global dialog_rows
    global dialog_columns
    global env
    global components

    puts "Create_workspace llamado"

    if {$frame == "."} { set base "" } else { set base $frame }
    set bw $base.widgets

    # If user wants to edit this, then the workspace has been already created.
    if ![string compare .$env(MC_EDIT) $frame] {
	return 0
    }

    create_gui_canvas $frame

    $bw  configure -relief sunken -borderwidth 2
    canvas $base.h -back white -height 8 -relief sunken -borderwidth 2
    canvas $base.v -back white -width 8 -relief sunken -borderwidth 2

    grid   $bw -column 1 -row 1 -sticky nwse -padx 2 -pady 2 -ipady 12
    grid   $base.h -column 1 -row 0 -sticky we
    grid   $base.v -column 0 -row 1 -sticky ns

    for {set col 0} {$col <= $dialog_columns} {incr col} {
	column_space $bw $col
	create_column $bw $col 1
    }
    for {set row 0} {$row <= $dialog_rows} {incr row} {
	row_space $bw $row
	create_row $bw $row 1
    }
}

proc get_stick {root widget} {
    global props

    set a $props(stick.n.$widget)
    set b $props(stick.s.$widget)
    set c $props(stick.e.$widget)
    set d $props(stick.w.$widget)
    return "$a$b$c$d"
}

#
# Callbacks for configuring widgets, frames and extra text
# 

    proc set_stick {root widget} {
	if {$root == "."} { set base "" } else { set base $root }
	grid $base.widgets.$widget -sticky [get_stick $root $widget]
    }

    proc make_sticky_button {root window widget sval} {
	checkbutton $window.$sval -text $sval -variable props(stick.$sval.$widget) \
	    -command "set_stick $root $widget" -onvalue $sval -offvalue ""
    }

 
    #
    # Configure a widget
    #
    proc config_widget {root widget} {
	global components
	global props

	set w .config-$widget

	toplevel $w
	frame $w.f
	make_sticky_button $root $w.f $widget n 
	make_sticky_button $root $w.f $widget s 
	make_sticky_button $root $w.f $widget e 
	make_sticky_button $root $w.f $widget w 
	label $w.f.l -text "Anchor"
	pack $w.f.l $w.f.n $w.f.s $w.f.e $w.f.w
	pack $w.f
    }

    proc make_radio_button {root window widget state} {
	radiobutton $window.$state -text $state -variable frame_relief -value $state \
	    -command "$root.widgets.$widget configure -relief $state" 
	pack $window.$state
    }
    #
    # Configure a frame
    #
    proc config_frame {root widget} {
	set w .config-$widget

	toplevel $w
	make_radio_button $root $w $widget sunken
	make_radio_button $root $w $widget groove
	make_radio_button $root $w $widget ridge
	make_radio_button $root $w $widget raised
    }

    proc set_text {root widget from} {
	set text [.config-$widget.f.entry get]
	puts "Texto: $text"
	$root.widgets.$widget configure -text $text
    }

    proc config_text {root widget} {
	config_widget $root $widget
	entry .config-$widget.f.entry -text [lindex [$root.widgets.$widget configure -text] 4]
	pack .config-$widget.f.entry
	bind .config-$widget.f.entry <Return> "set_text $root $widget .config-$widget.f.entry"
    }

    proc config_line {root widget} {
	# Nothing is configurable on a line.
    }

proc reconfig_rows {root} {
    global dialog_rows
    global dialog_columns

    for {set i 0} {$i < $dialog_rows} {incr i} {
	set cn [expr $i*2]
	grid $root.row@cn -columnspan [expr $dialog_columns*2+2]
    }
}

#
# Set the column for a widget
#
proc set_widget_col {root w col} {
    global dialog_columns

    if {$root == "."} { set base "" } else { set base $root }
    if {$col >= $dialog_columns} {
	return
    }
    grid $base.widgets.$w -column [expr $col*2+1]
}

#
# Set the row for a widget
#
proc set_widget_row {root w row} {
    global dialog_rows

    if {$root == "."} { set base "" } else { set base $root }
    if {$row >= $dialog_rows} {
	return
    }
    grid $base.widgets.$w -row [expr $row*2+1]
}

#
# Set the number of spanning lines for a widget
#
proc set_span_col {root w n} {
    if {$root == "."} { set base "" } else { set base $root }
    grid $base.widgets.$w -columnspan [expr $n*2-1]
}

#
# Set the number of spanning rows for a widget
#
proc set_span_row {root w n} {
    if {$root == "."} { set base "" } else { set base $root }
    grid $base.widgets.$w -rowspan [expr $n*2-1]
}

proc set_sticky {root w s} {
    global props

    if {$root == "."} { set base "" } else { set base $root }
    grid $base.widgets.$w -sticky $s

    foreach stick_dir {n s w e} {
	if [regexp $stick_dir $s] {
	    set props(stick.$stick_dir.$w) $stick_dir
	}
    }
}

#
# Start a drag
#
proc drag {root w x y} {
    global dragging
    global root_x
    global root_y

    if {$root == "."} { set base "" } else { set base $root }

    if {!$dragging} {
	set dragging 1
	button $base.widgets.drag -text "$w"
    } 
    place $base.widgets.drag -x [expr $x-$root_x] -y [expr $y-$root_y]
}

#
# Drop action
#
proc drop {root w x y} {
    global root_x
    global root_y

    global dragging

    if {$root == "."} { set base "" } else { set base $root }
    set pos [grid location $base.widgets [expr $x-$root_x] [expr $y-$root_y]]
    
    set col [expr [lindex $pos 0]/2]
    set row [expr [lindex $pos 1]/2]
    set_widget_row $root $w $row 
    set_widget_col $root $w $col
    set dragging 0
    catch "destroy $root.widgets.drag"
}

#
# Setup before the drag
#
proc button_press {root} {
    global root_x
    global root_y
    
    if {$root == "."} { set base "" } else { set base $root }
    set root_x [expr [winfo rootx $base.widgets]]
    set root_y [expr [winfo rooty $base.widgets]]
}

#
# Extract a value from a {key value ...} list returned by Tk
#
proc extract_parameter {parameters key} {
    return [lindex $parameters [expr [lsearch $parameters $key]+1]]
}

#
# Return the value of a variable stored in the props() array
#
proc get_prop {root win} {
    global props

    return $props($root.props.$win)
}

#
# Save the layout as defined by the user
#
proc save_gui {root dlg} {
    global dialog_columns
    global dialog_rows
    global components
    global frame_count
    global text_count
    global line_count

    if {$root == "."} { set base "" } else { set base $root }

    set file [open "gui$dlg.tcl" w]

    puts $file "set props($dlg.columns) $dialog_columns"
    puts $file "set props($dlg.rows)    $dialog_rows"
    puts $file "set props($dlg.frames)  $frame_count"
    puts $file "set props($dlg.texts)   $text_count"
    puts $file "set props($dlg.lines)   $line_count"

    set cnum [llength $components]
    puts $file "set props($dlg.components)   \"$components\""
    puts $file "set props($dlg.count)   $cnum"

    # 1. dump components

    foreach i $components {
	set winfo [grid info $base.widgets.$i]

	puts $file "set props($dlg.props.$i) \"$winfo\""
    }
    
    # 2. dump frames
    for {set i 0} {$i < $frame_count} {incr i} {
	set winfo [grid info $base.widgets.frame$i]
	set relief [lindex [$base.widgets.frame$i configure -relief] end]

	puts $file "set props($dlg.frame$i) \"$winfo\""
	puts $file "set props($dlg.relief.frame$i) $relief"
    }

    # 3. dump texts
    for {set i 0} {$i < $text_count} {incr i} {
	set winfo [grid info $base.widgets.text$i]
	set text  [lindex [$base.widgets.text$i configure -text] end]
	puts $file "set props($dlg.text$i) \"$winfo\""
	puts $file "set props($dlg.text.text$i) \"$text\""
    }

    # 4. dump lines
    for {set i 0} {$i < $line_count} {incr i} {
	set winfo [grid info $base.widgets.line$i]
	puts $file "set props($dlg.line$i) \"$winfo\""
    }
    close $file
}

#
# Setup the bindings for a given widget to make it drag and droppable
#
proc make_draggable {root wn short} {
    bind $wn <ButtonPress-1> "button_press $root; update idletasks"
    bind $wn <B1-Motion> "drag $root $short %X %Y; update idletasks"
    bind $wn <ButtonRelease-1> "drop $root $short %X %Y; update idletasks"
}

#
# root, window name, what = { frame, text, widget }
#
proc make_config_button {root i what} {
    if {$root == "."} { set base "" } else { set base $root } 

    frame .gui-widgets.$i
    button .gui-widgets.$i.button -command "config_$what $root $i " -text "$i"

    set    spans [grid info $base.widgets.$i] 
    scale  .gui-widgets.$i.scale-x -orient horizontal -from 1 -to 10 -label "span-x" \
	-command "set_span_col $root $i"
    scale  .gui-widgets.$i.scale-y -orient horizontal -from 1 -to 10 -label "span-y" \
	-command "set_span_row $root $i"

    .gui-widgets.$i.scale-y set [expr 1+([lindex $spans [expr 1+[lsearch $spans -rowspan]]]-1)/2]
    .gui-widgets.$i.scale-x set [expr 1+([lindex $spans [expr 1+[lsearch $spans -columnspan]]]-1)/2]
    pack   .gui-widgets.$i.button .gui-widgets.$i.scale-x .gui-widgets.$i.scale-y -side left
    pack   .gui-widgets.$i -side top
}

#
# Create a new border (these are widgets not known by mc)
#
proc new_border {root} {
    global frame_count
    if {$root == "."} { set base "" } else { set base $root }

    set short frame$frame_count
    set wn    $base.widgets.$short
    incr frame_count

    # create the frame
    frame $wn  -relief sunken -borderwidth 2 
    grid $wn -row 1 -column 1  -columnspan 1 -rowspan 1 -sticky wens -padx 2 -pady 2
    lower $wn

    # drag and dropability
    make_draggable $root $wn $short

    # configurability
    make_config_button $root $short frame
} 

#
# Create a new line separator (these are widgets not known by mc)
#
proc new_line {root} {
    global line_count
    if {$root == "."} { set base "" } else { set base $root }

    set short line$line_count
    set wn $base.widgets.$short
    incr line_count

    # create the line
    frame $wn -height 3 -bd 1 -relief sunken
    grid  $wn -row 1 -column 1 -columnspan 1 -rowspan 1 -sticky wens -padx 2 -pady 2
    lower $wn

    # drag and dropability
    make_draggable $root $wn $short
    
    # configurability
    make_config_button $root $short line
}

#
# Create a new text (these are widgets not known by mc)
#
proc new_text {root} {
    global text_count

    if {$root == "."} { set base "" } else { set base $root }

    set short text$text_count
    set wn    $base.widgets.$short
    incr text_count

    label $wn -text "Text..."
    grid $wn -row 1 -column 1 -columnspan 1 -rowspan 1 
    make_draggable $root $wn $short
    make_config_button $root $short text
}

#
# Start up the GUI designer
#

proc gui_design {root components} {
    global props
    global new_dialog

    # May be created in layout_with_grid if reconfiguring
    catch {toplevel .gui-widgets}

    if {$root == "."} {
	set base ""
    } else {
	set base $root
    }

    if {$new_dialog} {
	reset_counters
    }
    # Work around Tk 4.1 bug
    frame $base.widgets.bug-work-around
    grid $base.widgets.bug-work-around -row 60 -column 60

    foreach i $components {
	set def_layout [catch "get_prop $root $i" val]
	if {$def_layout} {
	    set_widget_col $root $i 0
	    set_widget_row $root $i 0
	} 
	make_draggable $root $base.widgets.$i $i
	make_config_button $root $i widget
    }
    frame .gui-widgets.buttons
    button .gui-widgets.buttons.save -text "Save to: gui$root.tcl" -command "save_gui $root $root"
    button .gui-widgets.buttons.abort -text "abort" -command "exit"
    button .gui-widgets.buttons.newf -text "New border" -command "new_border $root"
    button .gui-widgets.buttons.newl -text "New line"   -command "new_line $root"
    button .gui-widgets.buttons.newt -text "New text"   -command "new_text $root"

    pack\
	.gui-widgets.buttons.save \
	.gui-widgets.buttons.abort \
	.gui-widgets.buttons.newf \
	.gui-widgets.buttons.newt \
	-side left -expand y
    pack .gui-widgets.buttons
}

#
# Attempt to layout a grided dialog.  If something fails, return 0
# to give the application the chance to run the GUI designer
#
proc layout_with_grid {dialog count} {
    global props
    global components
    global min_width
    global min_height
    global env
    global dialog_columns
    global dialog_rows
    global frame_count
    global text_count
    global line_count
    global new_dialog

    set expr "set saved_count \$props(.\$dialog.count)"

    set new_dialog 1
    if [catch "eval $expr"] {
	puts "Calling editor, reason: count"
	return 0
    }
    set bw .$dialog.widgets

    if {$saved_count != $count} {
	puts "Calling editor, reason: more widgets"
	return 0
    } 

    set new_dialog 0

    # Check if the user wants to modify this dialog
    if ![string compare $env(MC_EDIT) $dialog] {
	set modify_dialog 1
	toplevel .gui-widgets
    } else {
	set modify_dialog 0
    }
    
    # First, hack around the crash problem of Tk 4.2 beta 1
    frame .$dialog.widgets.work-around
    grid .$dialog.widgets.work-around -row 60 -column 60

    set dialog_columns $props(.$dialog.columns)
    set dialog_rows    $props(.$dialog.rows)
    for {set i 0} {$i <= $dialog_columns} {incr i} {
	column_space $bw $i
	create_column $bw $i $modify_dialog
    }
    for {set i 0} {$i <= $dialog_rows} {incr i} {
	row_space $bw $i
	create_row $bw $i $modify_dialog
    }
    grid .$dialog.widgets -column 0 -row 0 -ipadx 8 -ipady 8 -sticky nswe

    # 1. Load the borders (first, because they may cover other widgets)
    set frame_count $props(.$dialog.frames)
    for {set i 0} {$i < $frame_count} {incr i} {
	frame .$dialog.widgets.frame$i -relief $props(.$dialog.relief.frame$i) -borderwidth 2
	eval grid .$dialog.widgets.frame$i "$props(.$dialog.frame$i)"
	if {$modify_dialog} {
	    lower .$dialog.widgets.frame$i
	    make_draggable .$dialog .$dialog.widgets.frame$i frame$i 
	    make_config_button .$dialog frame$i frame	    
	}
    }

    # 1.1 Load the lines (before texts, since they may cover other widgets)
    if {![catch {set line_count $props(.$dialog.lines)}]} {
	for {set i 0} {$i < $line_count} {incr i} {
	    frame .$dialog.widgets.line$i -relief sunken -bd 1 -height 3
	    eval grid .$dialog.widgets.line$i "$props(.$dialog.line$i)"
	    if {$modify_dialog} {
		lower .$dialog.widgets.line$i
		make_draggable .$dialog .$dialog.widgets.line$i line$i 
		make_config_button .$dialog line$i line
	    }
	}
    }

    # 2. Load the components
    foreach i $components {
	eval grid .$dialog.widgets.$i "$props(.$dialog.props.$i)"
	raise .$dialog.widgets.$i 
    }

    # 3 . Load the texts
    set text_count $props(.$dialog.texts)
    for {set i 0} {$i < $text_count} {incr i} {
	label .$dialog.widgets.text$i -text $props(.$dialog.text.text$i)
	eval grid .$dialog.widgets.text$i "$props(.$dialog.text$i)"
	raise .$dialog.widgets.text$i 
	if {$modify_dialog} {
	    make_draggable .$dialog .$dialog.widgets.text$i text$i 
	#    make_config_button .$dialog text$i text
	}
    }

    if {$modify_dialog} { 
	puts "Calling editor, reason: modify_dialog set"
	return 0 
    }
    return 1
}

#
# For testing the GUI builder.  Not used by the Midnight Commander
#
proc mc_create_buttons {root} {
    if {$root == "."} { set base "" } else { set base $root }

    button $base.widgets.button#1 -text "Oki\ndoki\nmy\friends"
    button $base.widgets.button#2 -text "Cancel"
    entry  $base.widgets.entry#1
    radiobutton $base.widgets.radio#1 -text "Primera opcion" 
    radiobutton $base.widgets.radio#2 -text "Segunda opcion"
    radiobutton $base.widgets.radio#3 -text "Tercera opcion"
}

proc test_gui {} {
    global components
    button .a -text "A" 
    pack .a
    
    toplevel .hola

    create_gui_canvas .hola

    set components {button#1 button#2 entry#1 radio#1 radio#2 radio#3}
    mc_create_buttons .hola

    if [layout_with_grid hola 6] {
	puts corriendo
    } else {
	create_workspace .hola
	gui_design .hola $components
    }
}

# initialize
reset_counters

if ![info exists env(MC_EDIT)] {
    set env(MC_EDIT) non-existant-toplevel-never-hit
}

if [catch {set x $mc_running}] { set mc_running 0 }

if {$use_separate_gui_files} {
    if [catch "glob gui.*.tcl" files] {
	set files ""
    }

    foreach i $files {
	puts "loading $i..."
	source $i
    }
} else {
    source $LIBDIR/gui.tcl
}

if {$mc_running == 0} {
    test_gui 
}
