  // *****************************************************************************
//
// Copyright (c) 2014, Southwest Research Institute® (SwRI®)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Southwest Research Institute® (SwRI®) nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// *****************************************************************************


#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include <mapviz/map_canvas.h>

#include <geometry_msgs/msg/point.h>
#include <swri_math_util/constants.h>


// C++ standard libraries
#include <cmath>
#include <list>
#include <memory>
#include <string>


namespace mapviz
{


bool compare_plugins(MapvizPluginPtr a, MapvizPluginPtr b)
{
  return a->DrawOrder() < b->DrawOrder();
}

/**
 * Convenience method for generating a geometry_msgs::msg::PointStamped in one line,
 * since we do this a few times.
 * @param x
 * @param y
 * @param z
 * @return
 */
geometry_msgs::msg::PointStamped make_point_stamped(double x, double y, double z)
{
  geometry_msgs::msg::PointStamped point;
  point.point.x = x;
  point.point.y = y;
  point.point.z = z;
  return point;
}

/**
 * Convenience method for converting a tf2::Stamped<tf2::Transform> object into
 * the equivalent ROS message.  This has to be done differently between ROS Dashing
 * and Eloquent, so this wraps it up conveniently.
 * @param transform The source object
 * @return That tf as a ROS message
 */
auto tf2_to_msg(const tf2::Stamped<tf2::Transform>& transform)
{
#if USE_NEW_TF2_TOMSG == 1
  return tf2::toMsg(transform);
#else
  return tf2::toMsg<tf2::Stamped<tf2::Transform>, geometry_msgs::msg::TransformStamped>(transform);
#endif
}

MapCanvas::MapCanvas(QWidget* parent) :
  QGLWidget(QGLFormat(QGL::SampleBuffers), parent),
  has_pixel_buffers_(false),
  pixel_buffer_size_(0),
  pixel_buffer_ids_(),
  pixel_buffer_index_(0),
  capture_frames_(false),
  initialized_(false),
  fix_orientation_(false),
  rotate_90_(false),
  enable_antialiasing_(true),
  mouse_button_(Qt::NoButton),
  mouse_pressed_(false),
  mouse_x_(0),
  mouse_y_(0),
  mouse_previous_y_(0),
  mouse_hovering_(false),
  mouse_hover_x_(0),
  mouse_hover_y_(0),
  offset_x_(0),
  offset_y_(0),
  drag_x_(0),
  drag_y_(0),
  view_center_x_(0),
  view_center_y_(0),
  view_scale_(1),
  view_left_(-25),
  view_right_(25),
  view_top_(10),
  view_bottom_(-10),
  scene_left_(-10),
  scene_right_(10),
  scene_top_(10),
  scene_bottom_(-10)
{
  RCLCPP_INFO(rclcpp::get_logger("mapviz"), "View scale: %f meters/pixel", view_scale_);
  setMouseTracking(true);


  QObject::connect(&frame_rate_timer_, SIGNAL(timeout()), this, SLOT(update()));
  setFrameRate(50.0);
  frame_rate_timer_.start();
  setFocusPolicy(Qt::StrongFocus);
}

MapCanvas::~MapCanvas()
{
  if (pixel_buffer_size_ != 0) {
    glDeleteBuffersARB(2, pixel_buffer_ids_);
  }
}

void MapCanvas::InitializeTf(std::shared_ptr<tf2_ros::Buffer> tf)
{
  tf_buf_ = tf;
}

void MapCanvas::InitializePixelBuffers()
{
  if (has_pixel_buffers_) {
    int32_t buffer_size = width() * height() * 4;

    if (pixel_buffer_size_ != buffer_size) {
      if (pixel_buffer_size_ != 0) {
        glDeleteBuffersARB(2, pixel_buffer_ids_);
      }

      glGenBuffersARB(2, pixel_buffer_ids_);
      glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_ids_[0]);
      glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, buffer_size, 0, GL_STREAM_READ_ARB);
      glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_ids_[1]);
      glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, buffer_size, 0, GL_STREAM_READ_ARB);
      glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);

      pixel_buffer_size_ = buffer_size;
    }
  }
}

void MapCanvas::initializeGL()
{
  GLenum err = glewInit();
  if (GLEW_OK != err) {
    RCLCPP_ERROR(rclcpp::get_logger("mapviz"), "Error: %s\n", glewGetErrorString(err));
  } else {
    // Check if pixel buffers are available for asynchronous capturing
    std::string extensions = (const char*)glGetString(GL_EXTENSIONS);
    has_pixel_buffers_ = extensions.find("GL_ARB_pixel_buffer_object") != std::string::npos;
  }

  glClearColor(0.58f, 0.56f, 0.5f, 1);
  if (enable_antialiasing_) {
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
  } else {
    glDisable(GL_MULTISAMPLE);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
  }
  initGlBlending();

  initialized_ = true;
}

void MapCanvas::initGlBlending()
{
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthFunc(GL_NEVER);
  glDisable(GL_DEPTH_TEST);
}

void MapCanvas::resizeGL(int w, int h)
{
  UpdateView();
}

void MapCanvas::CaptureFrame(bool force)
{
  // Ensure the pixel size is actually 4
  glPixelStorei(GL_PACK_ALIGNMENT, 4);

  if (has_pixel_buffers_ && !force) {
    InitializePixelBuffers();

    pixel_buffer_index_ = (pixel_buffer_index_ + 1) % 2;
    int32_t next_index = (pixel_buffer_index_ + 1) % 2;

    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_ids_[pixel_buffer_index_]);
    glReadPixels(0, 0, width(), height(), GL_BGRA, GL_UNSIGNED_BYTE, 0);
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pixel_buffer_ids_[next_index]);
    GLubyte* data = reinterpret_cast<GLubyte*>(
      glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB));
    if (data) {
      capture_buffer_.resize(pixel_buffer_size_);

      memcpy(&capture_buffer_[0], data, pixel_buffer_size_);

      glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
    }
    glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
  } else {
    int32_t buffer_size = width() * height() * 4;
    capture_buffer_.clear();
    capture_buffer_.resize(buffer_size);

    glReadPixels(0, 0, width(), height(), GL_BGRA, GL_UNSIGNED_BYTE, &capture_buffer_[0]);
  }
}

void MapCanvas::paintEvent(QPaintEvent* event)
{
  if (capture_frames_) {
    CaptureFrame();
  }

  QPainter p(this);
  p.setRenderHints(QPainter::Antialiasing |
                   QPainter::TextAntialiasing |
                   QPainter::SmoothPixmapTransform |
                   QPainter::HighQualityAntialiasing,
                   enable_antialiasing_);
  p.beginNativePainting();
  // .beginNativePainting() disables blending and clears a handful of other
  // values that we need to manually reset.
  initGlBlending();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();

  glClearColor(bg_color_.redF(), bg_color_.greenF(), bg_color_.blueF(), 1.0f);
  UpdateView();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  TransformTarget(&p);

  // Draw test pattern
  glLineWidth(3);
  glBegin(GL_LINES);
  // Red line to the right
  glColor3f(1, 0, 0);
  glVertex2f(0, 0);
  glVertex2f(20, 0);

  // Green line to the top
  glColor3f(0, 1, 0);
  glVertex2f(0, 0);
  glVertex2f(0, 20);
  glEnd();

  std::list<MapvizPluginPtr>::iterator it;
  for (it = plugins_.begin(); it != plugins_.end(); ++it) {
    // Before we let a plugin do any drawing, push all matrices and attributes.
    // This helps to ensure that plugins can't accidentally mess something up
    // for the next plugin.
    pushGlMatrices();

    (*it)->DrawPlugin(view_center_x_, view_center_y_, view_scale_);

    if ((*it)->SupportsPainting()) {
      p.endNativePainting();
      (*it)->PaintPlugin(&p, view_center_x_, view_center_y_, view_scale_);
      p.beginNativePainting();
      initGlBlending();
    }

    popGlMatrices();
  }

  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  p.endNativePainting();
}

void MapCanvas::pushGlMatrices()
{
  glMatrixMode(GL_TEXTURE);
  glPushMatrix();
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glPushAttrib(GL_ALL_ATTRIB_BITS);
}

void MapCanvas::popGlMatrices()
{
  glPopAttrib();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_TEXTURE);
  glPopMatrix();
}

void MapCanvas::wheelEvent(QWheelEvent* e)
{
  float numDegrees = e->delta() / -8;

  Zoom(numDegrees / 10.0);
}

void MapCanvas::Zoom(float factor)
{
  view_scale_ *= std::pow(1.1, factor);
  UpdateView();
}

void MapCanvas::mousePressEvent(QMouseEvent* e)
{
  mouse_x_ = e->x();
  mouse_y_ = e->y();
  mouse_previous_y_ = mouse_y_;
  drag_x_ = 0;
  drag_y_ = 0;
  mouse_pressed_ = true;
  mouse_button_ = e->button();
}

void MapCanvas::keyPressEvent(QKeyEvent* event)
{
  std::list<MapvizPluginPtr>::iterator it;
  for (it = plugins_.begin(); it != plugins_.end(); ++it) {
    (*it)->event(event);
  }
}

QPointF MapCanvas::MapGlCoordToFixedFrame(const QPointF& point)
{
  bool invertible = true;
  return qtransform_.inverted(&invertible).map(point);
}

QPointF MapCanvas::FixedFrameToMapGlCoord(const QPointF& point)
{
  return qtransform_.map(point);
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* e)
{
  mouse_button_ = Qt::NoButton;
  mouse_pressed_ = false;
  offset_x_ += drag_x_;
  offset_y_ += drag_y_;
  drag_x_ = 0;
  drag_y_ = 0;
}

void MapCanvas::mouseMoveEvent(QMouseEvent* e)
{
  if (mouse_pressed_ && canvas_able_to_move_) {
    int diff;
    switch (mouse_button_) {
      case Qt::LeftButton:
      case Qt::MiddleButton:
        if (((mouse_x_ - e->x()) != 0 || (mouse_y_ - e->y()) != 0)) {
          drag_x_ = -((mouse_x_ - e->x()) * view_scale_);
          drag_y_ = ((mouse_y_ - e->y()) * view_scale_);
        }
        break;
      case Qt::RightButton:
        diff = e->y() - mouse_previous_y_;
        if (diff != 0) {
          Zoom((static_cast<float>(diff)) / 10.0f);
        }
        mouse_previous_y_ = e->y();
        break;
      default:
        // Unexpected mouse button
        break;
    }
  }

  double center_x = -offset_x_ - drag_x_;
  double center_y = -offset_y_ - drag_y_;
  double x = center_x + (e->x() - width() / 2.0) * view_scale_;
  double y = center_y + (height() / 2.0 - e->y()) * view_scale_;

  geometry_msgs::msg::PointStamped point_in = make_point_stamped(x, y, 0.0);
  geometry_msgs::msg::PointStamped point_out;

  auto tfm_temp = tf2_to_msg(transform_);
  tf2::doTransform(point_in, point_out, tfm_temp);

  mouse_hovering_ = true;
  mouse_hover_x_ = e->x();
  mouse_hover_y_ = e->y();

  Q_EMIT Hover(point_out.point.x, point_out.point.y, view_scale_);
}

void MapCanvas::leaveEvent(QEvent* e)
{
  mouse_hovering_ = false;
  Q_EMIT Hover(0, 0, 0);
}

void MapCanvas::SetFixedFrame(const std::string& frame)
{
  fixed_frame_ = frame;
  std::list<MapvizPluginPtr>::iterator it;
  for (it = plugins_.begin(); it != plugins_.end(); ++it) {
    (*it)->SetTargetFrame(frame);
  }
}

void MapCanvas::SetTargetFrame(const std::string& frame)
{
  offset_x_ = 0;
  offset_y_ = 0;
  drag_x_ = 0;
  drag_y_ = 0;

  target_frame_ = frame;
}

void MapCanvas::ToggleFixOrientation(bool on)
{
  fix_orientation_ = on;
}

void MapCanvas::ToggleRotate90(bool on)
{
  rotate_90_ = on;
}

void MapCanvas::ToggleEnableAntialiasing(bool on)
{
  enable_antialiasing_ = on;
  QGLFormat format;
  format.setSwapInterval(1);
  format.setSampleBuffers(enable_antialiasing_);
  // After setting the format, initializeGL will automatically be called again, then paintGL.
  this->setFormat(format);
}

void MapCanvas::ToggleUseLatestTransforms(bool on)
{
  std::list<MapvizPluginPtr>::iterator it;
  for (it = plugins_.begin(); it != plugins_.end(); ++it) {
    (*it)->SetUseLatestTransforms(on);
  }
}

void MapCanvas::AddPlugin(MapvizPluginPtr plugin, int)
{
  plugins_.push_back(plugin);
}

void MapCanvas::RemovePlugin(MapvizPluginPtr plugin)
{
  plugin->Shutdown();
  plugins_.remove(plugin);
}

void MapCanvas::TransformTarget(QPainter* painter)
{
  glTranslatef(offset_x_ + drag_x_, offset_y_ + drag_y_, 0);
  // In order for plugins drawing with a QPainter to be able to use the same coordinates
  // as plugins using drawing using native GL commands, we have to replicate the
  // GL transforms using a QTransform.  Note that a QPainter's coordinate system is
  // flipped on the Y axis relative to OpenGL's.
  qtransform_ = qtransform_.translate(offset_x_ + drag_x_, -(offset_y_ + drag_y_));

  view_center_x_ = -offset_x_ - drag_x_;
  view_center_y_ = -offset_y_ - drag_y_;

  if (!tf_buf_ || fixed_frame_.empty() || target_frame_.empty() || target_frame_ == "<none>") {
    qtransform_ = qtransform_.scale(1, -1);
    painter->setWorldTransform(qtransform_, false);

    return;
  }

  bool success = false;

  try
  {
    auto tfrm = tf_buf_->lookupTransform(
      fixed_frame_,
      target_frame_,
      tf2::TimePointZero,
      std::chrono::seconds(1));

    tf2::fromMsg(tfrm, transform_);

    // If the viewer orientation is fixed don't rotate the center point.
    if (fix_orientation_) {
      transform_.setRotation(tf2::Transform::getIdentity().getRotation());
    }

    if (rotate_90_) {
      tf2::Quaternion yaw90;
      yaw90.setRPY(0, 0, -swri_math_util::_half_pi);
      transform_.setRotation(yaw90 * transform_.getRotation());
    }

    double roll, pitch, yaw;
    transform_.getBasis().getRPY(roll, pitch, yaw);

    glRotatef(-yaw * 57.2957795, 0, 0, 1);
    qtransform_ = qtransform_.rotateRadians(yaw);

    glTranslatef(-transform_.getOrigin().getX(), -transform_.getOrigin().getY(), 0);
    qtransform_ = qtransform_.translate(
      -transform_.getOrigin().getX(),
      transform_.getOrigin().getY());

    geometry_msgs::msg::PointStamped point = make_point_stamped(view_center_x_, view_center_y_, 0.0);
    geometry_msgs::msg::PointStamped center;

    auto tfm_temp = tf2_to_msg(transform_);
    tf2::doTransform(point, center, tfm_temp);

    view_center_x_ = center.point.x;
    view_center_y_ = center.point.y;

    qtransform_ = qtransform_.scale(1, -1);
    painter->setWorldTransform(qtransform_, false);

    if (mouse_hovering_) {
      double center_x = -offset_x_ - drag_x_;
      double center_y = -offset_y_ - drag_y_;
      double x = center_x + (mouse_hover_x_ - width() / 2.0) * view_scale_;
      double y = center_y + (height() / 2.0  - mouse_hover_y_) * view_scale_;

      geometry_msgs::msg::PointStamped hover_in = make_point_stamped(x, y, 0.0);
      geometry_msgs::msg::PointStamped hover_out;

      tfm_temp = tf2_to_msg(transform_);
      tf2::doTransform(hover_in, hover_out, tfm_temp);

      Q_EMIT Hover(hover_out.point.x, hover_out.point.y, view_scale_);
    }

    success = true;
  }
  catch (const tf2::LookupException& e)
  {
    RCLCPP_ERROR(rclcpp::get_logger("mapviz"), "%s", e.what());
  }
  catch (const tf2::ConnectivityException& e)
  {
    RCLCPP_ERROR(rclcpp::get_logger("mapviz"), "%s", e.what());
  }
  catch (const tf2::ExtrapolationException& e)
  {
    RCLCPP_ERROR(rclcpp::get_logger("mapviz"), "%s", e.what());
  }
  catch (...)
  {
    RCLCPP_ERROR(rclcpp::get_logger("mapviz"), "Error looking up transform");
  }

  if (!success)
  {
    qtransform_ = qtransform_.scale(1, -1);
    painter->setWorldTransform(qtransform_, false);
  }
}

void MapCanvas::UpdateView()
{
  if (initialized_) {
    Recenter();

    glViewport(0, 0, width(), height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(view_left_, view_right_, view_top_, view_bottom_, -0.5f, 0.5f);

    qtransform_ = QTransform::fromTranslate(width() / 2.0, height() / 2.0).
        scale(1.0 / view_scale_, 1.0 / view_scale_);
  }
}

void MapCanvas::ResetLocation()
{
  SetTargetFrame(target_frame_);
  SetViewScale(1.0);
}

void MapCanvas::ReorderDisplays()
{
  plugins_.sort(compare_plugins);
}

void MapCanvas::Recenter()
{
  // Recalculate the bounds of the view
  view_left_ = -(width() * view_scale_ * 0.5);
  view_top_ = -(height() * view_scale_ * 0.5);
  view_right_ = (width() * view_scale_ * 0.5);
  view_bottom_ = (height() * view_scale_ * 0.5);
}

void MapCanvas::setFrameRate(const double fps)
{
  if (fps <= 0.0) {
    RCLCPP_ERROR(rclcpp::get_logger("mapviz"), "Invalid frame rate: %f", fps);
    return;
  }

  frame_rate_timer_.setInterval(1000.0/fps);
}

double MapCanvas::frameRate() const
{
  return 1000.0 / frame_rate_timer_.interval();
}
}  // namespace mapviz
