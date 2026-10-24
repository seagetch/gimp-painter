static GdkEvent *
gimp_display_shell_compress_motion (GdkEvent  *initial_event,
                                    GdkEvent **next_event)
{
  GdkEvent  *last_motion = NULL;
  GtkWidget *widget;

  *next_event = NULL;

  if (initial_event->any.type != GDK_MOTION_NOTIFY)
    return NULL;

  widget = gtk_get_event_widget (initial_event);

  while (gdk_events_pending ())
    {
      GdkEvent *event = gdk_event_get ();

      if (!event)
        {
          /* Do nothing */
        }
      else if ((gtk_get_event_widget (event) == widget)               &&
               (event->any.type      == GDK_MOTION_NOTIFY)            &&
               (event->any.window    == initial_event->any.window)    &&
               (event->motion.state  == initial_event->motion.state)  &&
               (event->motion.device == initial_event->motion.device))
        {
          /* Discard previous motion event */
          if (last_motion)
            gdk_event_free (last_motion);

          last_motion = event;
        }
      else
        {
          /* Let the caller dispatch the event */
          *next_event = event;

          break;
        }
    }

  return last_motion;
}
