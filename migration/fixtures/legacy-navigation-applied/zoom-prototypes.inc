void
gimp_display_shell_scale (GimpDisplayShell *shell,
                          GimpZoomType      zoom_type,
                          gdouble           new_scale,
                          GimpZoomFocus     zoom_focus);
static void
gimp_display_shell_scale_to (GimpDisplayShell *shell,
                             gdouble           scale,
                             gint              viewport_x,
                             gint              viewport_y);
static void
gimp_display_shell_scale_get_zoom_focus (GimpDisplayShell *shell,
                                         gdouble           new_scale,
                                         gdouble           current_scale,
                                         gint             *x,
                                         gint             *y,
                                         GimpZoomFocus     zoom_focus);
static void
gimp_display_shell_scale_get_image_center_viewport (GimpDisplayShell *shell,
                                                    gint             *image_center_x,
                                                    gint             *image_center_y);
gboolean
gimp_display_shell_scale_image_is_within_viewport (GimpDisplayShell *shell,
                                                   gboolean         *horizontally,
                                                   gboolean         *vertically);
static gboolean
gimp_display_shell_scale_image_starts_to_fit (GimpDisplayShell *shell,
                                              gdouble           new_scale,
                                              gdouble           current_scale,
                                              gboolean         *vertically,
                                              gboolean         *horizontally);
static gboolean
gimp_display_shell_scale_image_stops_to_fit (GimpDisplayShell *shell,
                                             gdouble           new_scale,
                                             gdouble           current_scale,
                                             gboolean         *vertically,
                                             gboolean         *horizontally);
static gboolean
gimp_display_shell_scale_viewport_coord_almost_centered (GimpDisplayShell *shell,
                                                         gint              x,
                                                         gint              y,
                                                         gboolean         *horizontally,
                                                         gboolean         *vertically);
void
gimp_display_shell_calculate_scale_x_and_y (GimpDisplayShell *shell,
                                            gdouble           scale,
                                            gdouble          *scale_x,
                                            gdouble          *scale_y);
void
gimp_display_shell_scale_by_values (GimpDisplayShell *shell,
                                    gdouble           scale,
                                    gint              offset_x,
                                    gint              offset_y,
                                    gboolean          resize_window);
void
gimp_display_shell_scale_resize (GimpDisplayShell *shell,
                                 gboolean          resize_window,
                                 gboolean          grow_only);
void
gimp_display_shell_get_screen_resolution (GimpDisplayShell *shell,
                                          gdouble          *xres,
                                          gdouble          *yres);
void
gimp_display_shell_scroll_center_image (GimpDisplayShell *shell,
                                        gboolean          horizontally,
                                        gboolean          vertically);
void
gimp_display_shell_scroll_set_offset (GimpDisplayShell *shell,
                                      gint              offset_x,
                                      gint              offset_y);
void
gimp_display_shell_scroll_clamp_offsets (GimpDisplayShell *shell);
void
gimp_display_shell_scroll_clamp_and_update (GimpDisplayShell *shell);
void
gimp_display_shell_draw_get_scaled_image_size (GimpDisplayShell *shell,
                                               gint             *w,
                                               gint             *h);
void
gimp_display_shell_draw_get_scaled_image_size_for_scale (GimpDisplayShell *shell,
                                                         gdouble           scale,
                                                         gint             *w,
                                                         gint             *h);
void
gimp_display_shell_untransform_xy_f (const GimpDisplayShell *shell,
                                     gdouble                 x,
                                     gdouble                 y,
                                     gdouble                *nx,
                                     gdouble                *ny);
void
gimp_display_shell_set_cairo_rotate (const GimpDisplayShell *shell,
                                     cairo_t          *cr);
void
gimp_display_shell_scale_changed (GimpDisplayShell *shell);
void
gimp_zoom_model_zoom (GimpZoomModel *model,
                      GimpZoomType   zoom_type,
                      gdouble        scale);
gdouble
gimp_zoom_model_get_factor (GimpZoomModel *model);
gdouble
gimp_zoom_model_zoom_step (GimpZoomType zoom_type,
                           gdouble      scale);
static void
gimp_zoom_model_set_property (GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec);
