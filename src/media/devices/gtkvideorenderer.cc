// libjingle
// Copyright 2004 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Implementation of GtkVideoRenderer

#include "media/devices/gtkvideorenderer.h"

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <mutex>
#include <vector>

#include "media/base/videocommon.h"
#include "media/base/videoframe.h"

namespace cricket {

struct GtkWidgets {
  GtkWidget *window;
  GtkWidget *drawing_area;
  std::vector<uint8> argbPixels;
  int frameWidth, frameHeight;
};

GtkVideoRenderer::GtkVideoRenderer(int x, int y)
    : initial_x_(x),
      initial_y_(y) {

  widgets.reset(new GtkWidgets());
  widgets->window = NULL;
  widgets->drawing_area = NULL;
  widgets->frameWidth = 0;
  widgets->frameHeight = 0;
}

GtkVideoRenderer::~GtkVideoRenderer() {
  if (widgets->window) {
    gtk_widget_destroy(widgets->window);
    // Run the Gtk main loop to tear down the window.
    Pump();
  }
}

bool GtkVideoRenderer::SetSize(int width, int height, int reserved) {
  // For the first frame, initialize the GTK window
  if ((!widgets->window && !Initialize(width, height)) || IsClosed()) {
    return false;
  }

  gtk_window_resize(GTK_WINDOW(widgets->window), width, height);

  return true;
}

std::mutex mtx;

static gboolean OnDrawFrame(GtkWidget *widget, cairo_t *cr, GtkWidgets* widgets) {

  int x = 0;
  int y = 0;
  int h;
  int w;

  GdkRectangle allocation;
  allocation.width = gtk_widget_get_allocated_width (widgets->drawing_area);
  allocation.height = gtk_widget_get_allocated_height (widgets->drawing_area);

  GdkPixbuf* pxbscaled = NULL;
  {
    std::lock_guard<std::mutex> lck(mtx);
    
    if (!widgets->frameWidth || !widgets->frameHeight)
      return FALSE;
  
    const double aspect = (double)widgets->frameWidth / widgets->frameHeight;
    h = allocation.height;
    w = aspect * h;
    if (w > allocation.width)
    {
      w = allocation.width;
      h = w / aspect;
    }
    
    if (!w || !h)
      return FALSE;

    GBytes* gBytes = g_bytes_new_static(reinterpret_cast<uint8*>(&widgets->argbPixels[0]),
                                        widgets->argbPixels.size());

    GdkPixbuf* pixbuf = gdk_pixbuf_new_from_bytes(gBytes, GDK_COLORSPACE_RGB, TRUE, 8,
      widgets->frameWidth, widgets->frameHeight, widgets->frameWidth * 4);

    pxbscaled = gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_BILINEAR);

    g_object_unref(pixbuf);
  }

  if (w < allocation.width)
     x = (allocation.width - w) / 2;
  if (h < allocation.height)
     y = (allocation.height - h) / 2;

  gdk_cairo_set_source_pixbuf(cr, pxbscaled, x, y);
  cairo_rectangle(cr, x, y, x + w, y + h);
  cairo_fill(cr);

  g_object_unref(pxbscaled);

  return FALSE;
}

bool GtkVideoRenderer::RenderFrame(int width, int height, uint8* argbPixels_)
{
  // For the first frame, initialize the GTK window
  if ((!widgets->window && !Initialize(width, height)) || IsClosed()) {
    return false;
  }

  // Just exit, if locked - the next frame will be drawn.
  if (mtx.try_lock()) {
    widgets->frameWidth = width;
    widgets->frameHeight = height;

    if (widgets->argbPixels.size() < widgets->frameWidth * widgets->frameHeight * 4)
      widgets->argbPixels.resize(widgets->frameWidth * widgets->frameHeight * 4);

    memcpy(reinterpret_cast<uint8*>(&widgets->argbPixels[0]), argbPixels_, width * height * 4);
    
    mtx.unlock();
  }

  // Draw new frame.
  gtk_widget_queue_draw(widgets->drawing_area);

  // Pass through the gtk events queue.
  Pump();
  return true;
}
 
bool GtkVideoRenderer::RenderFrame(const VideoFrame* frame) {

  // For the first frame, initialize the GTK window
  if ((!widgets->window && !Initialize(frame->GetWidth(), frame->GetHeight())) || IsClosed()) {
    return false;
  }

  if (!frame)
    return false;

  // Just exit, if locked - the next frame will be drawn.
  if (mtx.try_lock()) {
    widgets->frameWidth = frame->GetWidth();
    widgets->frameHeight = frame->GetHeight();
  
    if (widgets->argbPixels.size() < widgets->frameWidth * widgets->frameHeight * 4)
      widgets->argbPixels.resize(widgets->frameWidth * widgets->frameHeight * 4);

    // convert I420 frame to ABGR format, which is accepted by GTK
    frame->ConvertToRgbBuffer(cricket::FOURCC_ABGR,
                              reinterpret_cast<uint8*>(&widgets->argbPixels[0]),
                              widgets->frameWidth * widgets->frameHeight * 4,
                              widgets->frameWidth * 4);
    mtx.unlock();
  }

  // Draw new frame.
  gtk_widget_queue_draw(widgets->drawing_area);

  // Pass through the gtk events queue.
  Pump();
  return true;
}

bool GtkVideoRenderer::Initialize(int width, int height) {
  gtk_init(NULL, NULL);
  widgets->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  widgets->drawing_area = gtk_drawing_area_new();

  gtk_container_add(GTK_CONTAINER(widgets->window), widgets->drawing_area);

  gtk_widget_set_size_request(GTK_WIDGET(widgets->window), 100, 100);
  gtk_window_set_resizable(GTK_WINDOW(widgets->window), TRUE);
  gtk_widget_show_all(GTK_WIDGET(widgets->window));

  g_signal_connect(widgets->drawing_area, "draw", G_CALLBACK(OnDrawFrame), widgets.get());
  return true;
}

void GtkVideoRenderer::Pump() {
  while (gtk_events_pending()) {
    gtk_main_iteration();
  }
}

bool GtkVideoRenderer::IsClosed() const {
  if (!widgets->window) {
    // Not initialized yet, so hasn't been closed.
    return false;
  }

  if (!GTK_IS_WINDOW(widgets->window) || !GTK_IS_DRAWING_AREA(widgets->drawing_area)) {
    return true;
  }

  return false;
}

}  // namespace cricket

