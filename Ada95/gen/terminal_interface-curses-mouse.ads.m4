------------------------------------------------------------------------------
--                                                                          --
--                           GNAT ncurses Binding                           --
--                                                                          --
--                      Terminal_Interface.Curses.Mouse                     --
--                                                                          --
--                                 S P E C                                  --
--                                                                          --
--  Version 00.91                                                           --
--                                                                          --
--  The ncurses Ada95 binding is copyrighted 1996 by                        --
--  Juergen Pfeifer, Email: Juergen.Pfeifer@T-Online.de                     --
--                                                                          --
--  Permission is hereby granted to reproduce and distribute this           --
--  binding by any means and for any fee, whether alone or as part          --
--  of a larger distribution, in source or in binary form, PROVIDED         --
--  this notice is included with any such distribution, and is not          --
--  removed from any of its header files. Mention of ncurses and the        --
--  author of this binding in any applications linked with it is            --
--  highly appreciated.                                                     --
--                                                                          --
--  This binding comes AS IS with no warranty, implied or expressed.        --
------------------------------------------------------------------------------
--  Version Control:
--  $Revision: 1.2 $
------------------------------------------------------------------------------
include(`Mouse_Base_Defs')
with System;
with Terminal_Interface.Curses;

package Terminal_Interface.Curses.Mouse is

   use type Terminal_Interface.Curses.Window;
   use type Terminal_Interface.Curses.Line_Position;
   use type Terminal_Interface.Curses.Column_Position;

   --  |=====================================================================
   --  | man page curs_mouse.3x
   --  |=====================================================================
   --  Please note, that in ncurses-1.9.9e documentation mouse support
   --  is still marked as experimental. So also this binding will change
   --  if the ncurses methods change.
   --
   type Event_Mask is private;
   No_Events  : constant Event_Mask;
   All_Events : constant Event_Mask;

   type Mouse_Button is (Left,     -- AKA: Button 1
                         Middle,   -- AKA: Button 2
                         Right,    -- AKA: Button 3
                         Button4,  -- AKA: Button 4
                         Control,  -- Control Key
                         Shift,    -- Shift Key
                         Alt);     -- ALT Key

   type Button_State is (Released,
                         Pressed,
                         Clicked,
                         Double_Clicked,
                         Triple_Clicked);

   type Mouse_Event is private;

   procedure Register_Reportable_Event
     (B    : in Mouse_Button;
      S    : in Button_State;
      Mask : in out Event_Mask);
   --  Stores the event described by the button and the state in the mask.
   --  Before you call this the first time, you should init the mask
   --  with the Empty_Mask constant

   function Start_Mouse (Mask : Event_Mask := All_Events)
                         return Event_Mask;
   --  Wraps mousemask()

   procedure End_Mouse;
   pragma Import (C, End_Mouse, "_nc_ada_unregister_mouse");
   --  Terminates the mouse

   function Get_Mouse return Mouse_Event;
   --  AKA: getmouse()

   procedure Get_Event (Event  : in  Mouse_Event;
                        Y      : out Line_Position;
                        X      : out Column_Position;
                        Button : out Mouse_Button;
                        State  : out Button_State);
   --  !!! Warning: X and Y are screen coordinates. Due to ripped of lines they
   --  may not be identical to window coordinates.

   procedure Unget_Mouse (Event : in Mouse_Event);
   --  AKA: ungetmouse()

   function Enclosed_In_Window (Win    : Window := Standard_Window;
                                Event  : Mouse_Event) return Boolean;
   --  AKA: wenclose()
   --  But : use event instead of screen coordinates.

   function Mouse_Interval (Msec : Natural := 200) return Natural;
   --  AKA: mouseinterval()

private
   type Event_Mask is new Interfaces.C.int;
   No_Events  : constant Event_Mask := 0;
   All_Events : constant Event_Mask := -1;

   type Mouse_Event is
      record
         Id      : Integer range Integer (Interfaces.C.short'First) ..
                                 Integer (Interfaces.C.Short'Last);
         X, Y, Z : Integer range Integer (Interfaces.C.int'First) ..
                                 Integer (Interfaces.C.int'Last);
         Bstate  : Event_Mask;
      end record;
   pragma Convention (C, Mouse_Event);
   pragma Pack (Mouse_Event);

include(`Mouse_Event_Rep')
   Generation_Bit_Order : constant System.Bit_Order := System.M4_BIT_ORDER;

end Terminal_Interface.Curses.Mouse;
