# Midnight Commander Tk code.
# Copyright (C) 1995, 1996, 1997 Miguel de Icaza
#
#
#   Todo:
#     Fix the internal viewer
#     Missing commands: mc_file_info, mc_open_with
#     The default buttons have a problem with the new frame around them: 
#     they don't display the focus.
#   
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#   
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

wm iconname . tkmc
wm title    . "Midnight Commander (TK edition)"

#
# Menu routines
#
proc create_top_menu {} {
    global top_menus
    global setup
    set top_menus ""

    frame .mbar -relief raised -bd 2

    pack .mbar -side top -fill x
}

#
# Widget: WMenu
#
proc create_menu {str topmenu} {
    global top_menus
    global setup

    menubutton .mbar.$topmenu -text $str -underline 0 -menu .mbar.$topmenu.menu
    pack .mbar.$topmenu -side left
    menu .mbar.$topmenu.menu -tearoff $setup(tearoff)
    set top_menus "$top_menus $topmenu"
}

proc create_mentry {topmenu entry cmd idx} {
    .mbar.$topmenu.menu add command -label "$entry" -command "$cmd $idx"
}

proc add_separator {topmenu} {
    .mbar.$topmenu.menu add separator
}

#
# Widget: WButton
#
proc newbutton {name cmd text isdef} {
    if $isdef {
	frame $name -relief sunken -bd 1 
	set name "$name.button"
    }
    button $name -text "$text" -command $cmd -justify left
    if $isdef {
	pack $name -padx 1m -pady 1m
    }
}

#
# Widget: WGauge
#
proc newgauge {win} {
    global setup

    canvas $win -height $setup(heightc) -width 250 -relief sunken -border 2
    $win create rectangle 0 0 0 0 -tags gauge -fill black -stipple gray50

    # So that we can tell the C code, which is the gauge size currently
    bind $win <Configure> "x$win %w"
}

# Used to show the gauge information
proc gauge_shown {win} {
    # $win configure -relief sunken -border 2
}

# Used to hide the gauge information.
proc gauge_hidden {win} {
    # $win configure -relief flat -border 0
    $win coords gauge 0 0 0 0
}

#
# Widget: WView
#
proc view_size {cmd w h} {
    global setup

    $cmd dim [expr $w/$setup(widthc)] [expr $h/$setup(heightc)]
}

proc newview {is_panel container winname cmd} {
    global setup 

    # FIXME: The trick to get the window without too much
    # movement/flicker is to use an extra frame, and use the placer
    # like it was done in WInfo.
    if $is_panel {
	set width  [expr [winfo width $container]/$setup(widthc)]
	set height [expr [winfo height $container]/$setup(heightc)]
    }

    frame $winname
    frame $winname.v
    frame $winname.v.status
    eval text $winname.v.view $setup(view_normal) -font $setup(panelfont) 
    
    # Create the tag names for the viewer attributes
    eval $winname.v.view tag configure bold $setup(view_bold)
    eval $winname.v.view tag configure underline $setup(view_underline)
    eval $winname.v.view tag configure mark $setup(view_mark)
    eval $winname.v.view tag configure normal $setup(view_normal)
    
    # Make the status fields
    label $winname.v.status.filename
    label $winname.v.status.column
    label $winname.v.status.size
    label $winname.v.status.flags
    
    pack $winname.v.status.filename -side left
    pack $winname.v.status.column   -anchor w -side left -fill x -expand 1
    pack $winname.v.status.size     -anchor w -side left -fill x -expand 1
    pack $winname.v.status.flags    -anchor w -side left -fill x -expand 1
    
    # Pack the main components
    pack $winname.v.status -side top -fill x
    pack $winname.v.view -side bottom -expand 1 -fill both
    pack $winname.v -expand 1 -fill both
    
    bindtags $winname.v.view "all . $winname.v.view"
    bind $winname.v.view <Configure> "view_size $cmd %w %h"

    if $is_panel {
	$winname.v.view configure -width $width -height $height
	pack $winname
    }
}

proc view_update_info {win fname col size flags} {
    $win.v.status.filename configure -text "File: $fname"
    $win.v.status.column   configure -text "Column: $col"
    $win.v.status.size     configure -text "$size bytes"
    $win.v.status.flags    configure -text "$flags"
}

#
# Hack: remove all the text on the window and then insert
# new lines.  Maybe the newlines
proc cleanview {win} {
    $win delete 1.0 end
    $win insert 1.0 "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n"
}

#
# Widget: WRadio
#
proc newradio {name} {
    frame $name
    global last_radio 
    set last_radio $name
}

proc radio_item {idx text cmd act} {
     global last_radio
     radiobutton $last_radio.$idx -text "$text" -variable v$last_radio -value $idx -command "$cmd select $idx"
     if $act {
	 $last_radio.$idx select 
     }
     pack $last_radio.$idx -side top -anchor w
}

#
# Widget: Input
#
#

proc entry_save_sel {win} {
    global sel

    if [$win selection present] {
	set sel(pres) 1
	set sel(first) [$win index sel.first]
	set sel(last)  [$win index sel.last]
    } else {
	set sel(pres) 0
    }
}

proc entry_restore_sel {win} {
    global sel

    if $sel(pres) {
	$win selection from $sel(first) 
	$win selection to $sel(last)
    }
}

proc entry_click {win x} {
    global sel
    set p [$win index @$x] 
    x$win mouse $p
    x$win setmark
    set sel(from) $p
}

proc entry_move {win x} {
    global sel
    set p [$win index @$x]
    $win selection from $sel(from) 
    $win selection to $p
    x$win mouse $p
}

proc bind_entry {win} {
    bind $win <1>         "entry_click $win %x"
    bind $win <B1-Motion> "entry_move $win %x"
}

proc newinput {name text} {
    entry $name -relief sunken -background white -foreground black
    $name insert 0 "$text"
    bindtags $name "all . $name "
    bind_entry $name
}

#
# Widget: WCheck
#
proc newcheck {name cmd text act} {
    checkbutton $name -text "$text" -command "$cmd" 
    if $act { $name select }
}

#
# Widget: WInfo
#
# window is the container name and widget name: .left.o2
# container is the container name: .left
#
    proc info_entry {win name text} {
	frame $win.b.finfo.$name
	label $win.b.finfo.$name.label -text "$text"
	label $win.b.finfo.$name.info  
	grid $win.b.finfo.$name.label -row 0 -column 0 -sticky we
	grid $win.b.finfo.$name.info -row 0 -column 1 -sticky w
	grid columnconfigure $win.b.finfo.$name 0 -minsize 100
    }

proc newinfo {container window version} {
    global setup

    set width  [winfo width $container]
    set height [winfo height $container]

    frame $window -width $width -height $height \
	-borderwidth [expr $setup(widthc)/2]

    frame $window.b
    frame $window.b.v
    frame $window.b.finfo -relief groove -borderwidth 2
    frame $window.b.fs -relief groove -borderwidth 2

    label $window.b.v.version -text "  The Midnight Commander $version  " \
	-relief groove
    pack  $window.b.v.version -fill x

    info_entry $window fname    "File:"
    info_entry $window location "Location:"
    info_entry $window mode     "Mode:"
    info_entry $window links    "Links:"
    info_entry $window owner    "Owner:"
    info_entry $window size     "Size:"
    info_entry $window created  "Created:"
    info_entry $window modified "Modified:"
    info_entry $window access   "Access:"

    pack  $window.b.finfo.fname \
	$window.b.finfo.location $window.b.finfo.mode $window.b.finfo.links \
	$window.b.finfo.owner $window.b.finfo.size $window.b.finfo.created \
	$window.b.finfo.modified $window.b.finfo.access \
	-side top -anchor w 

    label $window.b.fs.fsys
    label $window.b.fs.dev
    label $window.b.fs.type

    frame  $window.b.fs.free
    label  $window.b.fs.free.label
    newcanvas $window.b.fs.free.canvas

    pack $window.b.fs.free.label -side left
    pack $window.b.fs.free.canvas -side left

    frame  $window.b.fs.freeino
    label  $window.b.fs.freeino.label
    newcanvas $window.b.fs.freeino.canvas
    pack  $window.b.fs.freeino.label -side left
    pack  $window.b.fs.freeino.canvas -side left

    pack $window.b.fs.fsys \
	$window.b.fs.dev $window.b.fs.type \
	$window.b.fs.free \
	$window.b.fs.freeino -side top -anchor w -padx $setup(widthc)

    pack $window.b.v -side top -anchor w -fill x -expand 1

    pack $window.b.fs -side bottom -anchor w -fill x -expand 1

    pack $window.b.finfo -side top -anchor w \
	-fill x -expand 1

    pack $window.b

    pack $window -fill both -expand 1
#    pack $window.b
    place $window.b -in $window -relx 0 -rely 0 -relheight 1 -relwidth 1
}

proc info_bar {win percent} {
    global setup
    set w [winfo width $win]
    set s [expr (100-$percent)*$w/100]
    
    $win coords bar 0 0 $s 50
#    puts stderr "Width: $w $s\n\r"
}

proc info_none {win} {
    $win coords bar 0 0 0 0
    
}

proc newcanvas {win} {
    global setup
    canvas $win -height $setup(heightc) -relief sunken -border 2
    $win create rectangle 0 0 0 0 -tags bar -fill black -stipple gray50
}

proc infotext {win text} {
    $win configure -text $text
}

proc xinfotext {win text} {
    $win.info configure -text $text
}
    # w containes the window name *and* the .b frame (like: .left.o2.b)
    # FIXME: We should also display the rdev information
proc info_update {w
                  fname dev ino 
		  mode mode_oct 
		  links owner group 
                  have_blocks blocks 
		  size 
		  have_rdev rdev rdev2
		  create modify access
		  fsys dev type 
		  have_space avail percent total
		  have_ino   nfree inoperc inotot} {

    # 
    # Set all the text information
    # 
    xinfotext $w.finfo.fname    "$fname"
    xinfotext $w.finfo.location "${dev}h:${ino}h"
    xinfotext $w.finfo.mode     "$mode ($mode_oct)"
    xinfotext $w.finfo.links    "$links"
    xinfotext $w.finfo.owner    "$owner/$group"
    if $have_blocks {
	xinfotext $w.finfo.size     "$size ($blocks blocks)"
    } else {
	xinfotext $w.finfo.size     "$size"
    }
    xinfotext $w.finfo.created  "$create"
    xinfotext $w.finfo.modified "$modify"
    xinfotext $w.finfo.access   "$access"
   
    infotext $w.fs.fsys "Filesystem:\t$fsys"
    infotext $w.fs.dev  "Device:\t\t$dev"
    infotext $w.fs.type "Type:\t\t$type"
    if $have_space {
	infotext $w.fs.free.label \
	   "Free Space $avail ($percent%) of $total"
	info_bar $w.fs.free.canvas $percent
    } else {
	infotext $w.fs.free.label "No space information"
	info_none $w.fs.free.canvas
    }
    if $have_ino {
	infotext $w.fs.freeino.label \
	   "Free inodes $nfree ($inoperc%) of $inotot"
	info_bar $w.fs.freeino.canvas $inoperc
    } else {
	infotext $w.fs.freeino.label "No inode information"
	info_none $w.fs.freeino.canvas
    }
}

#
#  Widget: listbox
#
proc listbox_sel {win item} {
    $win selection clear 0 end
    $win selection set $item
    $win see $item
}

#
# Widget: WPanel
#
proc panel_select {w pos cback} {
    $w.m.p.panel tag add selected $pos.0 "$pos.0 lineend"
    $w.m.p.panel see $pos.0
    $cback top [$w.m.p.panel index @0,0]
}

proc panel_scroll {win cback args} {
    eval "$win yview $args"
    $cback top [$win index @0,0]
}

proc cmd_sort_add {name menu cmd} {
    $menu add command -label $name -command "$cmd sort $name"
}

proc panel_setup {which cmd} {
    global setup

    frame $which
    set m $which.cwd.menu
    menubutton $which.cwd -text "loading..." -bd 1 -relief raised \
	-menu $m -indicatoron 0
    menu $m -tearoff 0
        menu [set mm $m.sort] -tearoff 0
        $m add command -label "Reverse sort order" -command "$cmd reverse"
        $m add cascade -label "Sort" -menu $mm
            cmd_sort_add "Name"  $mm $cmd
            cmd_sort_add "Extension" $mm $cmd
            cmd_sort_add "Size"  $mm $cmd
            cmd_sort_add "Modify Time" $mm $cmd
            cmd_sort_add "Access Time" $mm $cmd
            cmd_sort_add "Change Time" $mm $cmd
            cmd_sort_add "Inode" $mm $cmd
            cmd_sort_add "Type"  $mm $cmd
            cmd_sort_add "Links" $mm $cmd
            cmd_sort_add "NGID"  $mm $cmd
            cmd_sort_add "NUID"  $mm $cmd
            cmd_sort_add "Owner" $mm $cmd
            cmd_sort_add "Group" $mm $cmd
            cmd_sort_add "Unsorted" $mm $cmd

	$m add command -label "Refresh" -command "$cmd refresh"
	$m add separator
    	$m add command -label "Set mask" -command "$cmd setmask"
	$m add command -label "No mask"  -command "$cmd nomask"

    frame $which.m
    label $which.mini
    scrollbar $which.m.scroll -width 3m

    frame $which.m.p -relief sunken -borderwidth 2
        # The sort bar
        if $setup(with_sortbar) {
	    canvas $which.m.p.types \
		    -borderwidth 0\
		    -back $setup(def_back) \
		    -highlightthickness 0 -height 0
	    pack $which.m.p.types  -side top -fill x
	}

        scrollbar $which.m.p.scroll -width 3m -orient horizontal

        # The file listing panel
        text  $which.m.p.panel -width $setup(cols) -yscroll "$which.m.scroll set" \
	    -fore $setup(def_fore) -back $setup(def_back) \
   	    -wrap none -height $setup(lines) -font $setup(panelfont)  \
	    -relief flat -borderwidth 0 -highlightthickness 0 \
	    -xscroll "$which.m.p.scroll set"

        bindtags $which.m.p.panel "all . $which.m.p.panel"

	proc x$which.m.p.panel {x} "$cmd \$x"
	
        pack $which.m.p.panel  -side top  -fill both -expand 1
        pack $which.m.p.scroll -side top  -fill x 
        pack $which.m.p        -side left -fill both -expand 1

    pack $which.m.scroll -side right -fill y
    pack $which.cwd  -side top -anchor w
    pack $which.m    -side top -fill both -expand 1
    pack $which.mini -side top -fill x

    pack $which -fill both -expand 1

    config_colors $which.m.p.panel
}

    #
    # Draging the panels:
    # mc_x and mc_y contains the last positions where the mouse was
    # mc_repeat     contains the id of the after command.
    #
proc panel_cancel_repeat {} {
    global mc_repeat

    after cancel $mc_repeat
    set mc_repeat {}
}

proc panel_drag {w cmd n} {
    global mc_y
    global mc_x
    global mc_repeat

    if {$mc_y >= [winfo height $w]} {
	$w yview scroll 1 units 
    } elseif {$mc_y < 0} {
	$w yview scroll -1 units
    } else {
	return
    }
    $cmd top [$w index @0,0]
    $cmd motion $n [$w index @$mc_x,$mc_y]
    set mc_repeat [after 50 panel_drag $w $cmd $n]
}

    #
    #  This routine passes the size of the text widget back to the C code
    #

proc panel_size {cmd panel w h} {
    global setup

    set setup(real_height) $h
    set setup(real_width)  $w

    if $setup(with_icons) {
	set setup(height) [expr $h-setup(iconheight)]
	set setup(width)  [expr $w-$setup(iconwidth)]
	set setup(lines)  [expr $setup(height)/$setup(heightc)]
	set setup(cols)   [expr $setup(width)/$setup(widthc)]
    } else {
	set setup(height) $h
	set setup(width)  $w
	set setup(lines)  [expr $h/$setup(heightc)]
	set setup(cols)   [expr $w/$setup(widthc)]
    }
    $cmd resize $panel
}

    #
    # Called on the first idle loop to configure the sizes of the thing
    #
proc panel_conf {panel cmd} {
    global setup

    set font [lindex [$panel configure -font] 4]
    set fontinfo [$cmd fontdim $font $panel]

    set setup(heightc) [lindex $fontinfo 0]
    set setup(widthc)  [lindex $fontinfo 1]

    bind $panel <Configure> "panel_size $cmd $panel %w %h"
}

#
# Manage the bar that keeps the sort orders 
#
proc panel_reset_sort_labels {win} {
#    $win.m.p.types delete all
}

proc panel_sort_label_start {win} {
#    $win.m.p.types delete all
}

#
# This right now uses a canvas, creates labels and places them 
# on the canvas.  The button emulation is done with a manual 
# bind.
#

proc panel_add_sort {win text_len text pos end_pos tag} {
    global setup

    if {$setup(with_sortbar) == 0} return

    catch { destroy $win.m.p.types.$pos }
    label $win.m.p.types.$pos -text $text -borderwidth 2 \
	    -font $setup(panelfont) -relief raised

    $win.m.p.types create window $pos 0 -window $win.m.p.types.$pos -anchor nw
    $win.m.p.types configure -height [expr $setup(heightc)+8]
    $win.m.p.types create line $end_pos 1 $end_pos [expr $setup(heightc)-1] -fill gray

    # Simulate the button.
    bind $win.m.p.types.$pos <Button-1> "
        $win.m.p.types.$pos configure -relief sunken
        x$win sort $tag
        after 100 { $win.m.p.types.$pos configure -relief raised }
     "
}

#
# Called back from the action menu
#
proc popup_add_action {filename cmd idx} {
    global setup
    menu [set m .m.action] -tearoff 0
        .m add cascade -foreground $setup(action_foreground) -label "$filename" -menu $m
        $m add command -label "Info"            -command "mc_file_info \"$filename\""
        $m add command -label "Open with..."    -command "mc_open_with \"$filename\""
	$m add separator
	$m add command -label "Copy..."         -command "$cmd invoke %d Copy"
	$m add command -label "Rename, move..." -command "$cmd invoke %d Move"
	$m add command -label "Delete..."       -command "$cmd invoke %d Delete"
	$m add separator 
	$m add command -label "Open"            -command "$cmd invoke %d Open"
	$m add command -label "View"            -command "$cmd invoke %d View"
}

proc start_drag {mode panel_cmd W x y X Y} {
    global drag_mode
    global drag_text

    set drag_mode $mode
    set drag_text [$panel_cmd dragtext [$W index @$x,$y]]
    set drag_text "$mode $drag_text"

    catch {destroy .drag}
    toplevel .drag
    wm overrideredirect .drag 1
    wm withdraw .drag
    label .drag.text -text "$drag_text"
    pack .drag.text
    wm deiconify .drag
    wm geometry .drag +$X+$Y
}

proc drag_test {token} {
    if {[winfo children $token] == ""} {
	label $token.value -text "Zonzo"
	pack $token.value
    }
    $token.value configure -text Hla
    return "caca";
}

proc mc_drag_target {} {
    global DragDrop

    set data $DragDrop(text)
}

proc mc_drag_send {} {
#    puts "drag send"
}

    #
    # Mouse bindings for the panels
    #
proc panel_bind {the_panel panel_cmd} {
    global setup

    set pn "$the_panel.m.p.panel"

    bind $pn <Button-1> "
        $panel_cmd mdown 2  \[%W index @%x,%y]
    "
 
    bind $pn <ButtonRelease-1> "
        panel_cancel_repeat
        $panel_cmd mup 2    \[%W index @%x,%y]"

    bind $pn <Double-1> "
        panel_cancel_repeat
        $panel_cmd double 2 \[%W index @%x,%y]"

    bind $pn <B1-Motion> "
       set mc_x %x
       set mc_y %y
       $panel_cmd motion 2 \[%W index @%x,%y]
    "

    bind $pn <B1-Leave> "
        set mc_x %x
        set mc_y %y
	panel_drag %W $panel_cmd 2
    "

    bind $pn <B1-Enter> panel_cancel_repeat

    if $setup(b2_marks) {
	bind $pn <Button-2> "
            $panel_cmd mdown 1  \[%W index @%x,%y]"

	bind $pn <ButtonRelease-2> "
	    $panel_cmd mup 1    \[%W index @%x,%y]
	    panel_cancel_repeat
	"

	bind $pn <B2-Motion> "
	    set mc_x %x
	    set mc_y %y
	    $panel_cmd motion 1 \[%W index @%x,%y]
	"

	bind $pn <B2-Leave> "
	    set mc_x %x
	    set mc_y %y
	    panel_drag %W $panel_cmd 1
	"
    } else {
	# We have blt
#	blt_drag&drop source $pn config -button 2 -packagecmd drag_test \
#		-selftarget 1
#	blt_drag&drop source $pn handler text dd_send_file
    }

    # Menu popup.
    bind $pn <Button-3> "
	$panel_cmd mdown 2 \[%W index @%x,%y]
	catch {destroy .m}
	menu .m -tearoff 0
	$panel_cmd load \[%W index @%x,%y] %X %Y
	#Buggy Tk8.0
	catch {tk_popup .m %X %Y}
    "

    bind $pn <B2-Enter> panel_cancel_repeat

    $the_panel.m.scroll configure \
        -command "panel_scroll $pn $panel_cmd"
    $the_panel.m.p.scroll configure \
	-command "$pn xview"
    panel_conf $pn $panel_cmd
}

proc panel_info {item} {
    global setup

    return $setup($item)
}

proc panel_mark {tag panel n} {
    config_colors $panel
    $panel tag add $tag "${n}.0" "${n}.0 lineend"
}

# op is add or remove
proc panel_mark_entry {win op line} {
    $win.m.p.panel tag $op marked $line.0 "$line.0 lineend"
}

proc panel_unmark_entry {win line} {
    $win.m.p.panel tag remove selected $line.0 "$line.0 lineend"
}

#
# Misc routines
#

# Configure the panel tags
proc config_colors {which} {
    global setup

    # se -- selected file
    $which tag configure selected -back $setup(panelcolor,selected_back)

    foreach v {marked directory executable regular selected} {
	$which tag configure $v -fore $setup(panelcolor,$v)
    }
    $which tag configure directory -font $setup(paneldir)

    $which tag raise marked
}

proc tclerror {msg} {
 puts stderr "TkError: [$msg]\n\r"
}

#
# FIXME: This is not finished, have to deal with activefore, activeback
# highlight{fore,back}
#
proc error_colors {wins} {
    global setup

    foreach widget $wins {
	catch "$widget configure -foreground $setup(errorfore)"
	catch "$widget configure -background $setup(errorback)"
    }
}

#
#
# Layout routines
#
#
proc layout_midnight {} {
    global one_window
    global wlist

    #puts $wlist
    #
    # we want to make the prompt and the input line sunken
    # so we sunk the frame, and set a borderwidth for it
    # while removing the sunken attribute set by the newinput
    .p.i0 configure -relief flat
    .p configure -relief sunken -borderwidth 2
    pack .p.l5 -side left 
    pack .p.i0 -side left -expand 1 -fill x -anchor e
    pack .n4 -side bottom -fill x
    pack .p -side bottom -fill x

    if $one_window {
	pack .left -side top -side left -fill both -expand 1
	pack .right -side top -side right -fill both -expand 1
    }
}

proc layout_query {} {
    global wlist

#    puts "$wlist"
    set t [llength $wlist]
    if {$t == 2} {
	pack .query.l1 -side top -pady 2m -padx 2m
	pack .query.b0 -side right  -ipadx 2m -padx 4m -pady 2m -expand 1
    } else {
	pack .query.l1  -side top -pady 2m -padx 2m

	for {set b 2} {$b != 1} {incr b} {
	    if {$b == $t} {
		set b 0
	    }
	    pack .query.b$b -side right -ipadx 2m -padx 4m -pady 2m -expand 1
	}
    }
}

proc layout_listbox {} {
    scrollbar .listbox.s -width 3m -command {.listbox.x0 yview}
    .listbox.x0 configure -yscroll {.listbox.s set} 
    pack .listbox.s -fill y -side right
    pack .listbox.x0 -expand 1 -fill both -padx 4m -pady 4m -side left
}

proc layout_quick_confirm {} {
    pack .quick_confirm.c.c1 .quick_confirm.c.c2 .quick_confirm.c.c3 \
	-side top -anchor w
    pack .quick_confirm.b.b0 .quick_confirm.b.b4 -side left -padx 4m -expand 1
    
    pack .quick_confirm.c -side top -pady 4m
    pack .quick_confirm.b -side top -pady 2m
}

proc layout_quick_file_mask {} {
    global wlist
#    puts stderr "$wlist"

    # We add some space
    .quick_file_mask configure -borderwidth 5m

    pack .quick_file_mask.b.b1 .quick_file_mask.b.b2 \
	-side left -expand 1 -padx 4m

    pack .quick_file_mask.l3  -side top -anchor w -expand 1
    pack .quick_file_mask.s.i5 -fill x -expand 1 -anchor w
    pack .quick_file_mask.s.c6 -anchor e -padx 4m -pady 1m
    pack .quick_file_mask.s -expand 1 -fill x -side top

    pack .quick_file_mask.d.l7 -pady 4m
    pack .quick_file_mask.d -side top -anchor w
    pack .quick_file_mask.i4 -expand 1 -fill x -side top

    pack .quick_file_mask.t.c8 .quick_file_mask.t.c0 -side top -anchor e
    catch {pack .quick_file_mask.t.c9 -side top -anchor e}

    pack .quick_file_mask.t -side top -anchor e
    frame .quick_file_mask.space -height 4m
    pack .quick_file_mask.space -side top 
    pack .quick_file_mask.b -fill x -expand 1 -side top
}

proc layout_quick_vfs {} {
    global wlist
#    puts stderr "$wlist"
    pack .quick_vfs.t.l1 -side left
    pack .quick_vfs.t.i2 -side left -expand 1 -fill x -padx 2m
    pack .quick_vfs.t.l3 -side left

    pack .quick_vfs.l.l4 -side top -anchor w
    pack .quick_vfs.l.r5 -side left -anchor w
    pack .quick_vfs.l.i6 -side right -anchor se
    pack .quick_vfs.b.b7 .quick_vfs.b.b0 -padx 4m -side left -expand 1 

    pack .quick_vfs.t -side top -expand 1 -fill x -pady 4m -padx 4m
    pack .quick_vfs.l -side top -expand 1 -fill x -padx 4m
    pack .quick_vfs.b -side top -expand 1 -fill x -padx 4m -pady 4m
}

proc layout_dbits {} { 
    pack .dbits.r1 -anchor w -padx 4m -pady 4m -side top
    pack .dbits.b0 -side top
}

proc layout_chown {} {
    global setup

    pack .chown.b.b8 .chown.b.b0 -side left -padx 4m -expand 1

    # May be invoked with different number of buttons
    # There is already a problem: the cancel button is
    # not close to the ok button, I will have to look into this.
    catch { 
	pack .chown.b.b9 .chown.b.b10 .chown.b.b11 \
	    -side left -padx 4m -expand 1
    }
    label .chown.l.fname -text {File name}
    label .chown.l.owner -text {Owner name}
    label .chown.l.group -text {Group name}
    label .chown.l.size -text {Size}
    label .chown.l.perm -text {Permission}
    
    pack \
	.chown.l.fname .chown.l.l7 \
	.chown.l.owner .chown.l.l6 \
	.chown.l.group .chown.l.l5 \
	.chown.l.size  .chown.l.l4 \
	.chown.l.perm  .chown.l.l3 -side top -anchor w -padx 2m
	
    foreach i {l3 l4 l5 l6 l7} {
	.chown.l.$i configure -fore $setup(high)
    }
    pack .chown.l.l3 .chown.l.l4 .chown.l.l5 .chown.l.l6 .chown.l.l7 \
	-side top -pady 1m -padx 4m -anchor w

    # Configure the listboxes
    scrollbar .chown.f.s -width 3m -command {.chown.f.x2 yview}
    .chown.f.x2 configure -yscroll {.chown.f.s set}
    label .chown.f.l -text {Group name}
    pack .chown.f.l -side top -anchor w
    pack .chown.f.x2 -side left -fill y -expand 1
    pack .chown.f.s -side right -fill y -expand 1

    scrollbar .chown.g.s -width 3m -command {.chown.g.x1 yview}
    .chown.g.x1 configure -yscroll {.chown.g.s set}
    label .chown.g.l -text {User name}
    pack .chown.g.l -side top -anchor w
    pack .chown.g.x1 -side left -fill y -expand 1
    pack .chown.g.s -side right -fill y -expand 1

    .chown.b configure -relief sunken
    pack .chown.b -side bottom -pady 4m -fill x
    pack .chown.g .chown.f -side left -padx 4m -pady 4m -expand 1 -fill y
    pack .chown.l -side right -padx 4m
}

proc layout_chmod {} {
    global wlist
#    puts stderr "$wlist \n\r"
    pack .chmod.c.c5 .chmod.c.c6 .chmod.c.c7 .chmod.c.c8 .chmod.c.c9 \
	.chmod.c.c10 \
	.chmod.c.c11 .chmod.c.c12 .chmod.c.c13 .chmod.c.c14 .chmod.c.c15 \
	.chmod.c.c16 -side top -anchor w

    pack .chmod.b.b17 .chmod.b.b0 -side left -padx 4m -pady 4m -side left
    catch {
	pack .chmod.b.b18 .chmod.b.b19 .chmod.b.b19 .chmod.b.b20 \
	    .chmod.b.b21 -side left -padx 4m -pady 4m -side left
    }

    label .chmod.l.msg -text {Use "t" or Insert to\nmark attributes}
    label .chmod.l.fname -text {Name}
    label .chmod.l.perm   -text {Permission (octal)}
    label .chmod.l.owner  -text {Owner name}
    label .chmod.l.group  -text {Group name}

    pack \
	.chmod.l.fname .chmod.l.l4 \
	.chmod.l.perm  .chmod.l.l1 \
	.chmod.l.owner .chmod.l.l3 \
	.chmod.l.group .chmod.l.l2 .chmod.l.msg -side top -anchor w -padx 2m
    pack .chmod.b -side bottom
    pack .chmod.l -side right -padx 4m -anchor n -pady 4m
    pack .chmod.c -side left -padx 4m -pady 4m
}

proc layout_view {} {
    global wlist

    pack [lindex $wlist 0] -side bottom -fill x
    pack [lindex $wlist 1] -side top -expand 1 -fill both
}

proc layout_replace {} {
    global wlist

    error_colors "$wlist .replace"

    set alist {}
    set plist {}
    set ilist {}

    foreach a $wlist {
	if [regexp ^.replace.p.l $a] {
	    set plabel $a
	} elseif [regexp ^.replace.p $a] {
	    set plist "$plist $a"
	} elseif [regexp ^.replace.a.l $a] {
	    set alabel $a
	} elseif [regexp ^.replace.a $a] {
	    set alist "$alist $a"
	} elseif [regexp ^.replace.i $a] {
	    set ilist "$ilist $a"
	} elseif [regexp ^.replace.b $a] {
	    set abortbutton $a
	} else {
	    set fname $a
	}
    }

#    puts stderr "$wlist\n\r"
#    puts stderr "alist: $alist\n\rplist: $plist\n\rilist: $ilist\n\r"
#    puts stderr "plabel: $plabel\n\rfname: $fname"

    pack $fname -side top -fill x -anchor w -pady 6m -padx 12m
    pack $abortbutton -side bottom -anchor e -padx 8m -pady 4m
    
    eval pack $ilist -side top -anchor w 
    pack .replace.i -padx 10m -pady 2m -anchor w

    pack $plabel -side left -anchor w -padx 10m
    eval pack $plist -side left -anchor e -fill x
    pack .replace.p -side top -fill x -padx 10m
    
    pack $alabel -side left -anchor w -padx 10m
    eval pack $alist -side left -anchor e -fill x
    pack .replace.a -side top -fill x  -padx 10m
}

proc layout_complete {} {
    global wlist

    eval pack $wlist -side top
}

proc layout_opwin {} {
    global wlist
    global setup

    pack .opwin.b.b0 .opwin.b.b14 -side left -expand 1
    pack .opwin.f0.l1 .opwin.f0.l2 -side left -anchor w
    pack .opwin.f1.l3 .opwin.f1.l4 -side left -anchor w

    foreach a {.opwin.2.l11 .opwin.1.l8 .opwin.0.l5} {
	$a configure -width 8
    }
    pack .opwin.2.l11 .opwin.2.g13 -side left -fill x
    pack .opwin.1.l8 .opwin.1.g10  -side left -fill x
    pack .opwin.0.l5 .opwin.0.l6  -side left -fill x

    pack .opwin.b -side bottom -pady 4m -fill x
    pack .opwin.f0 -side top -padx 10m -anchor w
    pack .opwin.f1 -side top -padx 10m -pady 4m -anchor w
    pack .opwin.0 .opwin.1 .opwin.2 -side top -padx 4m
}

proc dummy_layout {name} {
    eval "proc layout_$name {} {
          global wlist
#          puts stderr \"\$wlist \\n\"
          eval pack \$wlist -side top}"
}
#
# the achown commands will have to be rewriten
# to use only widgets and no writing callbacks.
#

foreach i {
    achown tree 
    } {
	dummy_layout $i
    }

proc layout_quick_input {} {
    global wlist

#    puts stderr "$wlist \n\r"
    .quick_input.i1 configure -width 60
    label .quick_input.dummy
    pack .quick_input.b.b2 .quick_input.b.b3 -side left -padx 4m -expand 1
    pack .quick_input.b -side bottom -pady 4m
    pack .quick_input.dummy -side top
    pack .quick_input.l0 -side top -expand 1 -ipadx 2m -ipady 2m
    pack .quick_input.i1 -side bottom -fill x -padx 4m
}

proc create_drop_target {name icon} {
    toplevel .drop-$name

    button .drop-$name.b -text "Drop target"
    pack .drop-$name.b
    wm group .drop-$name .
    wm overrideredirect .drop-$name 1
    wm withdraw .drop-$name
    wm deiconify .drop-$name
    wm geometry .drop-$name +0+0
#    blt_drag&drop target .drop-$name.b handler file mc_drag_target
#    blt_drag&drop target .drop-$name.b handler text mc_drag_target
}

#
# Creates the container
#
proc create_container {container} {
    canvas $container 
    pack $container -fill both -expand 1
}

#
# Removes all of the widgets in a container (.left or .right)
#
proc container_clean {container} {
    set widgets [winfo children $container]

    foreach widget $widgets {
	destroy $widget
    }
}

#
# Setups the binding called after the layout procedure
#
proc bind_setup {win} {
    flush stderr
    bindtags $win {all $win}
    wm protocol $win WM_DELETE_WINDOW "tkmc e scape"
    bind $win <Leave> 
}

proc keyboard_bindings {} {
    # Remove the Tab binding.
    bind all <Tab> {}

    bind all <KeyPress>          "tkmc r %A"

    # Remove the Alt-key binding and put a sensible one instead
    bind all <Alt-KeyPress>      "tkmc a %A"
    bind all <Control-KeyPress> "tkmc c %A"
    bind all <Meta-KeyPress>    "tkmc a %A"

    foreach i {Left Right Up Down End R13 Home F27 F29 Prior \
	       Next F35 Return KP_Enter Delete Insert BackSpace \
	       F1 F2 F3 F4 F5 F6 F7 F8 F9 F10} {
        bind all <Key-$i> "tkmc k %K"
    }
}

# Centers a window based on .
proc center_win {win} {
    global center_toplevels

    wm transient $win [winfo toplevel [winfo parent $win]]

    wm withdraw $win
    update idletasks

    if {$center_toplevels} {
	set ch [winfo reqheight $win]
	set cw [winfo reqwidth $win]
	
	set geo [split [wm geometry .] +x]
	set pw [lindex $geo 0]
	set ph [lindex $geo 1]
	set px [lindex $geo 2]
	set py [lindex $geo 3]
	
	set x [expr $px+(($pw-$cw)/2)]
	set y [expr $py+(($ph-$ch)/2)]
	
	wm geometry $win +$x+$y
    }
    wm deiconify $win
    grab $win
    tkwait visibility $win
}

#
# Busy window handling
#
proc win_busy {w} {
    $w configure -cursor watch
}


#
# Color configurations
#
proc tk_colors {} {
}

proc color_model {} {
}

# gray85 is the background for the new tk4
proc gray_colors {base} {
    global setup
    global have_blt

#
#    set setup(def_back) [tkDarken $base 90]
#    set setup(def_fore) black
#    set setup(selected) [tkDarken $base 110]
#    set setup(marked)   SlateBlue
#    set setup(high)     $setup(def_back)

    if {0} {
	set dark_color [tkDarken $base 90]
	set setup(def_back) [tkDarken $base 110]
	set setup(selected) NavyBlue
	set setup(selected_fg) white
    } else {
	set dark_color $base
	set setup(def_back) #d9d9d9
	set setup(selected) white
	set setup(selected_fg) black
    }
    set setup(def_fore) black
    set setup(high)     yellow

#
# Panel colors:
#
    # Marked files
    set setup(panelcolor,marked)        yellow
    set setup(panelcolor,directory)     blue
    set setup(panelcolor,executable)    red
    set setup(panelcolor,regular)       black
    set setup(panelcolor,selected_back) white
    set setup(panelcolor,selected)      black

# Viewer colors

    set setup(view_bold)      "-fore yellow -back $dark_color"
    set setup(view_underline) "-fore red    -back $dark_color"
    set setup(view_mark)      "-fore cyan   -back $dark_color"
    set setup(view_normal)    "-fore black  -back $dark_color"

# The percentage bars on info:
    set setup(percolor) "blue"

# The sort bar colors
    set setup(sort_fg) $setup(def_fore)
    set setup(sort_bg) $setup(def_back)
    set setup(with_sortbar) 1

    # We use BLT only for drag and drop, if this is not available, 
    # then we use the 2nd button for regular file marking.
    set have_blt 0
    if $have_blt {
	set setup(b2_marks) 0
    } else {
	set setup(b2_marks) 1
    }

# The errors
    set setup(errorfore) white
    set setup(errorback) red

}

proc bisque_colors {} {
    global setup

    set setup(def_back) bisque3
    set setup(def_fore) black
    set setup(selected) bisque2
    set setup(marked)   SlateBlue
    set setup(high)     gray
}

proc sanity_check {} {
    if [catch {bindtags .}] {
	puts stderr "The Midnight Commander requires Tk 4.0 beta 3 or 4\n\r"
	puts stderr "You can get it from: ftp://ftp.smli.com/pub/tcl"
	exit 1
    }
}

#sanity_check

# Until I figure out how to remove specific bindings from a widget
# I remove all of the classes bindings.

#bind Text <Enter> {}
#bind Text <FocusIn> {}

# bind Entry <Enter> {}
# bind Entry <FocusIn> {}

keyboard_bindings 

set setup(tearoff) 0
set setup(action_foreground) blue
set setup(lines) 24
set setup(cols)  40
set setup(with_icons) 0
set setup(widthc) 0
set setup(heightc) 0
set setup(real_width) 0

# Determine Tk version
set beta_4 ![catch tk_bisque]
if $beta_4 { 
    tk_setPalette gray85
    gray_colors gray70
} else {
    bisque_colors 
}

## Some globals
set mc_repeat {}
set mc_x 0
set mc_y 0
set center_toplevels 1
#set center_toplevels 0

# button .testbutton [lindex [.testbutton configure -font] 3]
#set setup(panelfont) lucidasanstypewriter-bold-14
set setup(panelfont) lucidasanstypewriter-14
set setup(paneldir)  lucidasanstypewriter-bold-14
set setup(font)  "-*-helvetica-medium-r-normal-*-14-*-*-*-*-*-*-*"

#
# 
# This variable if set, will make the program load gui.*.tcl files
# instead of the gui.tcl file created during instalaltion.
set use_separate_gui_files 0

if [file exist ~/.mc/tkmc] {source ~/.mc/tkmc}

catch {
#    option add *font $setup(font) userDefault
#    option add *Menu*activeBackground NavyBlue
#    option add *Menu*activeForeground white
#    option add *Menubutton*activeBackground NavyBlue
#    option add *Menubutton*activeForeground white
#    option add *Button*activeBackground NavyBlue
#    option add *Button*activeForeground white
##    set setup(panelfont) $setup(font)
}

proc run_gui_design {root} {
    global components 

    create_workspace $root
    gui_design $root $components
}

source $LIBDIR/gd.tcl

set tk_strictMotif 1
create_top_menu

create_drop_target Hola Zonzo
