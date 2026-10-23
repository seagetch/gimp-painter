static int seq;
static void emit(GimpDisplayShell *s,const char *scene,const char *step,unsigned state,double x,double y) {
 cairo_surface_t *surf=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t *cr=cairo_create(surf);cairo_matrix_t m;
 double px=s->disp_width/2+40,py=s->disp_height/2+20,bx,by;
 gimp_display_shell_set_cairo_rotate(s,cr);cairo_get_matrix(cr,&m);cairo_user_to_device(cr,&px,&py);bx=px;by=py;cairo_device_to_user(cr,&bx,&by);
 printf("%d\t%s\t%s\t%u\t%.17g\t%.17g\t%d\t%d\t%d\t%.17g\t%.17g\t%d\t%d\t%d\t%d\t%d\t%d\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\t%.17g\n",++seq,scene,step,state,x,y,s->disp_width,s->disp_height,s->mirrored,s->rotate_angle,s->rotate_start_angle,s->rotating,s->grab,s->cursor,s->expose,s->draw,s->scale_requests,s->scale,m.xx,m.yx,m.xy,m.yy,m.x0,m.y0,px,py,bx,by);
 cairo_destroy(cr);cairo_surface_destroy(surf);
}
int main(void){
 const double initial[]={0,7.499999,7.5,7.500001,-7.499999,-7.5,-7.500001,15,22.5,-22.5,352.5,359.999,15-0.000001,15.000001,-15,-14.999999,-15.000001,345};
 puts("seq\tscene\tstep\tstate\tx\ty\twidth\theight\tmirror\tangle\tanchor\trotating\tgrab\tcursor\texpose\tdraw\tscale_requests\tscale\txx\tyx\txy\tyy\tx0\ty0\tpoint_x\tpoint_y\tback_x\tback_y");
 for(int w=100;w<=101;w++)for(int mirror=0;mirror<2;mirror++)for(int a=0;a<18;a++){
 char scene[80];GimpDisplayShell s={0,w,w+12,mirror,initial[a],0};GimpStatusbar bar={&s};GtkWidget toggle={1-mirror};GdkEvent e={0};int x=w/2,y=(w+12)/2+40;
 snprintf(scene,sizeof(scene),"stationary-w%d-m%d-a%d",w,mirror,a);emit(&s,scene,"setup",0,x,y);
 gimp_display_shell_start_rotating(&s,NULL,x,y);emit(&s,scene,"start",1,x,y);
 unsigned states[]={1,5,4,0,4,12,8,0};const char *names[]={"free","ctrl-add","shift-remove","ctrl-remove","ctrl-readd","alt-add","ctrl-remove-alt","alt-remove"};
 for(int i=0;i<8;i++){e.state=states[i];gimp_display_shell_rotate(&s,&e,x,y);emit(&s,scene,names[i],e.state,x,y);}
 gimp_display_shell_stop_rotating(&s,NULL);emit(&s,scene,"stop",0,x,y);
 s.rotate_angle=initial[a];gimp_statusbar_rotate_left_clicked(NULL,&bar);emit(&s,scene,"left",0,0,0);
 s.rotate_angle=initial[a];gimp_statusbar_rotate_right_clicked(NULL,&bar);emit(&s,scene,"right",0,0,0);
 s.rotate_angle=initial[a];gimp_statusbar_flip_side_clicked(&toggle,&bar);emit(&s,scene,"flip",0,0,0);
 gimp_statusbar_rotate_right_clicked(NULL,&bar);emit(&s,scene,"right-after-flip",0,0,0);
 gimp_statusbar_reset_view_clicked(NULL,&bar);emit(&s,scene,"reset",0,0,0);
 }
 const double points[][2]={{40,0},{0,-40},{-40,0},{26.9,100.9},{-26.9,100.9},{0,0},{40,40},{-40,-40},{0,40}};
 for(int w=100;w<=101;w++)for(int mirror=0;mirror<2;mirror++){
 char scene[80];GimpDisplayShell s={0,w,w+12,mirror,0,0};GdkEvent e={0};int cx=w/2,cy=(w+12)/2;
 snprintf(scene,sizeof(scene),"motion-w%d-m%d",w,mirror);gimp_display_shell_start_rotating(&s,NULL,cx,cy+40);emit(&s,scene,"start",13,cx,cy+40);
 for(int p=0;p<9;p++)for(int ctrl=0;ctrl<2;ctrl++){char step[50];double x=cx+points[p][0],y=cy+points[p][1];e.state=ctrl?4:0;snprintf(step,sizeof(step),"p%d-%s",p,ctrl?"snap":"free");gimp_display_shell_rotate(&s,&e,x,y);emit(&s,scene,step,e.state,x,y);}
 gimp_display_shell_stop_rotating(&s,NULL);emit(&s,scene,"stop",0,cx,cy+40);
 }
 return 0;
}
