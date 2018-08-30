#include <dwl_rviz_plugin/WholeBodyTrajectoryDisplay.h>

#include <boost/bind.hpp>

#include <OgreSceneNode.h>
#include <OgreSceneManager.h>
#include <OgreManualObject.h>
#include <OgreBillboardSet.h>
#include <OgreMatrix4.h>

#include <tf/transform_listener.h>

#include <rviz/display_context.h>
#include <rviz/frame_manager.h>
#include <rviz/properties/enum_property.h>
#include <rviz/properties/color_property.h>
#include <rviz/properties/float_property.h>
#include <rviz/properties/int_property.h>
#include <rviz/properties/vector_property.h>
#include <rviz/validate_floats.h>

#include <rviz/ogre_helpers/billboard_line.h>
#include <rviz/ogre_helpers/axes.h>


using namespace rviz;

namespace dwl_rviz_plugin
{

WholeBodyTrajectoryDisplay::WholeBodyTrajectoryDisplay() : is_info_(false)
{
	// Category Groups
	base_category_ = new rviz::Property("Base", QVariant(), "", this);
	contact_category_ = new rviz::Property("End-Effector", QVariant(), "", this);

	// Base trajectory properties
	base_style_property_ =
			new EnumProperty("Line Style", "Points",
							 "The rendering operation to use to draw the grid lines.",
							 base_category_, SLOT(updateBaseStyle()), this);

	base_style_property_->addOption("Points", POINTS);
	base_style_property_->addOption("Billboards", BILLBOARDS);
	base_style_property_->addOption("Lines", LINES);


	base_line_width_property_ =
			new FloatProperty("Line Width", 0.01,
							  "The width, in meters, of each path line. "
							  "Only works with the 'Billboards' and 'Points' style.",
							  base_category_, SLOT(updateBaseLineProperties()), this);
	base_line_width_property_->setMin(0.001);
	base_line_width_property_->show();

	base_color_property_ =
			new ColorProperty("Line Color", QColor(0, 85, 255),
							  "Color to draw the path.",
							  base_category_, SLOT(updateBaseLineProperties()), this);

	base_scale_property_ =
			new FloatProperty("Axes Scale", 1.0,
							  "The scale of the axes that describe the orientation.",
							  base_category_, SLOT(updateBaseLineProperties()), this);

	base_alpha_property_ =
			new FloatProperty("Alpha", 1.0,
							  "Amount of transparency to apply to the trajectory.",
							  base_category_, SLOT(updateBaseLineProperties()), this);
	base_alpha_property_->setMin(0);
	base_alpha_property_->setMax(1);


	// End-effector trajectory properties
	contact_style_property_ =
			new EnumProperty("Line Style", "Points",
							 "The rendering operation to use to draw the grid lines.",
							 contact_category_, SLOT(updateContactStyle()), this);

	contact_style_property_->addOption("Points", POINTS);
	contact_style_property_->addOption("Billboards", BILLBOARDS);
	contact_style_property_->addOption("Lines", LINES);


	contact_line_width_property_ =
			new FloatProperty("Line Width", 0.01,
							  "The width, in meters, of each trajectory line. "
							  "Only works with the 'Billboards' and 'Points' style.",
							  contact_category_, SLOT(updateContactLineProperties()), this);
	contact_line_width_property_->setMin(0.001);
	contact_line_width_property_->show();

	contact_color_property_ =
			new ColorProperty("Line Color", QColor(0, 85, 255),
							  "Color to draw the trajectory.",
							  contact_category_, SLOT(updateContactLineProperties()), this);

	contact_alpha_property_ =
			new FloatProperty("Alpha", 1.0,
							  "Amount of transparency to apply to the trajectory.",
							  contact_category_, SLOT(updateContactLineProperties()), this);
	contact_alpha_property_->setMin(0);
	contact_alpha_property_->setMax(1);
}


WholeBodyTrajectoryDisplay::~WholeBodyTrajectoryDisplay()
{
	destroyObjects();
}


void WholeBodyTrajectoryDisplay::onInitialize()
{
	MFDClass::onInitialize();
}


void WholeBodyTrajectoryDisplay::fixedFrameChanged()
{
	if (is_info_) {
		// Visualization of the base trajectory
		processBaseTrajectory();

		// Visualization of the end-effector trajectory
		processContactTrajectory();
	}
}


void WholeBodyTrajectoryDisplay::reset()
{
	MFDClass::reset();
}


void WholeBodyTrajectoryDisplay::updateBaseStyle()
{
	LineStyle style = (LineStyle) base_style_property_->getOptionInt();

	switch (style)
	{
	case LINES:
		base_line_width_property_->hide();
		base_billboard_line_.reset();
		base_points_.clear();
		break;
	case BILLBOARDS:
		base_line_width_property_->show();
		base_manual_object_.reset();
		base_points_.clear();
		break;
	case POINTS:
		base_line_width_property_->show();
		base_manual_object_.reset();
		base_billboard_line_.reset();
		break;
	}

	if (is_info_)
		processBaseTrajectory();
}


void WholeBodyTrajectoryDisplay::updateBaseLineProperties()
{
	LineStyle style = (LineStyle) base_style_property_->getOptionInt();
	float line_width = base_line_width_property_->getFloat();
	float scale = base_scale_property_->getFloat();
	Ogre::ColourValue color = base_color_property_->getOgreColor();
	color.a = base_alpha_property_->getFloat();


	if (style == BILLBOARDS) {
		base_billboard_line_->setLineWidth(line_width);
		base_billboard_line_->setColor(color.r, color.g, color.b, color.a);
		uint32_t num_axes = base_axes_.size();
		for (uint32_t i = 0; i < num_axes; i++) {
			Ogre::ColourValue x_color = base_axes_[i]->getDefaultXColor();
			Ogre::ColourValue y_color = base_axes_[i]->getDefaultYColor();
			Ogre::ColourValue z_color = base_axes_[i]->getDefaultZColor();
			x_color.a = base_alpha_property_->getFloat();
			y_color.a = base_alpha_property_->getFloat();
			z_color.a = base_alpha_property_->getFloat();
			base_axes_[i]->setXColor(x_color);
			base_axes_[i]->setYColor(y_color);
			base_axes_[i]->setZColor(z_color);
			base_axes_[i]->getSceneNode()->setVisible(true);
			base_axes_[i]->setScale(Ogre::Vector3(scale, scale, scale));
		}
	} else if (style == LINES) {
		// we have to process again the base trajectory
		if (is_info_)
			processBaseTrajectory();
	} else {
		uint32_t num_points = base_points_.size();
		for (uint32_t i = 0; i < num_points; i++) {
			base_points_[i]->setColor(color.r, color.g, color.b, color.a);
			base_points_[i]->setRadius(line_width);
		}

		uint32_t num_axes = base_axes_.size();
		for (uint32_t i = 0; i < num_axes; i++) {
			Ogre::ColourValue x_color = base_axes_[i]->getDefaultXColor();
			Ogre::ColourValue y_color = base_axes_[i]->getDefaultYColor();
			Ogre::ColourValue z_color = base_axes_[i]->getDefaultZColor();
			x_color.a = base_alpha_property_->getFloat();
			y_color.a = base_alpha_property_->getFloat();
			z_color.a = base_alpha_property_->getFloat();
			base_axes_[i]->setXColor(x_color);
			base_axes_[i]->setYColor(y_color);
			base_axes_[i]->setZColor(z_color);
			base_axes_[i]->getSceneNode()->setVisible(true);
			base_axes_[i]->setScale(Ogre::Vector3(scale, scale, scale));
		}
	}

	context_->queueRender();
}


void WholeBodyTrajectoryDisplay::updateContactStyle()
{
	LineStyle style = (LineStyle) contact_style_property_->getOptionInt();
	uint32_t num_contact = contact_billboard_line_.size();
	uint32_t num_points = contact_points_.size();

	switch (style)
	{
	case LINES:
		contact_line_width_property_->hide();
		for (uint32_t i = 0; i < num_contact; i++) {
			contact_billboard_line_[i].reset();
		}
		for (uint32_t i = 0; i < num_points; i++) {
			uint32_t num_elem = contact_points_[i].size();
			for (uint32_t j = 0; j < num_elem; j++)
				contact_points_[i][j].reset();
			contact_points_[i].clear();
		}
		break;
	case BILLBOARDS:
		contact_line_width_property_->show();
		for (uint32_t i = 0; i < num_contact; i++) {
			contact_manual_object_[i].reset();
		}
		for (uint32_t i = 0; i < num_points; i++) {
			uint32_t num_elem = contact_points_[i].size();
			for (uint32_t j = 0; j < num_elem; j++)
				contact_points_[i][j].reset();
			contact_points_[i].clear();
		}
		break;
	case POINTS:
		contact_line_width_property_->show();
		for (uint32_t i = 0; i < num_contact; i++) {
			contact_manual_object_[i].reset();
			contact_billboard_line_[i].reset();
		}
	}

	if (is_info_)
		processContactTrajectory();
}


void WholeBodyTrajectoryDisplay::updateContactLineProperties()
{
	LineStyle style = (LineStyle) contact_style_property_->getOptionInt();
	float line_width = contact_line_width_property_->getFloat();
	Ogre::ColourValue color = contact_color_property_->getOgreColor();
	color.a = contact_alpha_property_->getFloat();

	if (style == BILLBOARDS) {
		uint32_t num_contact = contact_billboard_line_.size();
		for (uint32_t i = 0; i < num_contact; i++) {
			contact_billboard_line_[i]->setLineWidth(line_width);
			contact_billboard_line_[i]->setColor(color.r, color.g, color.b, color.a);
		}
	} else if (style == LINES){
		// we have to process again the contact trajectory
		if (is_info_)
			processContactTrajectory();
	} else {
		uint32_t num_points = contact_points_.size();
		for (uint32_t i = 0; i < num_points; i++) {
			uint32_t num_contacts = contact_points_[i].size();
			for (uint32_t j = 0; j < num_contacts; j++) {
				contact_points_[i][j]->setColor(color.r, color.g, color.b, color.a);
				contact_points_[i][j]->setRadius(line_width);
			}
		}
	}

	context_->queueRender();
}


void WholeBodyTrajectoryDisplay::processMessage(const dwl_msgs::WholeBodyTrajectory::ConstPtr& msg)
{
	// Updating the message
	msg_ = msg;
	is_info_ = true;

	// Destroy all the old elements
	destroyObjects();

	// Visualization of the base trajectory
	processBaseTrajectory();

	// Visualization of the end-effector trajectory
	processContactTrajectory();
}


void WholeBodyTrajectoryDisplay::processBaseTrajectory()
{
	// Lookup transform into fixed frame
	Ogre::Vector3 position;
	Ogre::Quaternion orientation;
	if (!context_->getFrameManager()->getTransform(msg_->header, position, orientation)) {
		ROS_DEBUG("Error transforming from frame '%s' to frame '%s'",
				  msg_->header.frame_id.c_str(), qPrintable(fixed_frame_));
	}
	Ogre::Matrix4 transform(orientation);
	transform.setTrans(position);

	// Visualization of the base trajectory
	// Getting the base trajectory style
	LineStyle base_style = (LineStyle) base_style_property_->getOptionInt();

	// Getting the base trajectory color
	Ogre::ColourValue base_color = base_color_property_->getOgreColor();
	base_color.a = base_alpha_property_->getFloat();

	// Visualization of the base trajectory
	uint32_t num_points = msg_->trajectory.size();
	uint32_t num_base = msg_->trajectory[0].base.size();

	base_axes_.clear();
	switch (base_style)
	{
	case LINES: {
		base_manual_object_.reset(scene_manager_->createManualObject());
		base_manual_object_->setDynamic(true);
		scene_node_->attachObject(base_manual_object_.get());

		base_manual_object_->estimateVertexCount(num_points);
		base_manual_object_->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
		for (uint32_t i = 0; i < num_points; ++i) {
			Ogre::Vector3 pos(0., 0., 0.);
			Ogre::Quaternion quat;
			Eigen::Vector3d rpy(0., 0., 0.);
			for (uint32_t j = 0; j < num_base; j++) {
				dwl_msgs::BaseState base = msg_->trajectory[i].base[j];
				if (base.id == dwl::rbd::LX)
					pos.x = base.position;
				else if (base.id == dwl::rbd::LY)
					pos.y = base.position;
				else if (base.id == dwl::rbd::LZ)
					pos.z = base.position;
				else if (base.id == dwl::rbd::AX)
					rpy(0) = base.position;
				else if (base.id == dwl::rbd::AY)
					rpy(1) = base.position;
				else
					rpy(2) = base.position;
			}

                        //sanity check position
                        if (!(std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z)))
                        {
                            std::cerr<<"whole body traj position is not finite, resetting to zero!" <<std::endl;
                            pos.x = 0.0;
                            pos.y = 0.0;
                            pos.z = 0.0;
                        }
                        //sanity check orientation
                        if (!(std::isfinite(rpy(0)) && std::isfinite(rpy(1)) && std::isfinite(rpy(2))))
                        {
                            std::cerr<<"whole body traj orientation is not finite, resetting to quat 1 0 0 0!" <<std::endl;
                            rpy(0) = 0.0;
                            rpy(1) = 0.0;
                            rpy(2) = 0.0;
                        }

			Ogre::Vector3 xpos = transform * pos;
			base_manual_object_->position(xpos.x, xpos.y, xpos.z);
			base_manual_object_->colour(base_color);


			// We are keeping a vector of CoM frame pointers. This creates the next
			// one and stores it in the vector
			float scale = base_scale_property_->getFloat();
			if (i == 0 || i == num_points - 1) {
				last_point_position_ = xpos;

				// Adding the initial frame
				Eigen::Quaterniond q = dwl::math::getQuaternion(rpy);
				quat.w = q.w();
				quat.x = q.x();
				quat.y = q.y();
				quat.z = q.z();

                                boost::shared_ptr<rviz::Axes> axes;
				axes.reset(new Axes(scene_manager_, scene_node_, 0.04, 0.008));
				axes->setPosition(xpos);
				axes->setOrientation(quat * orientation); // combination of rotations
				Ogre::ColourValue x_color = axes->getDefaultXColor();
				Ogre::ColourValue y_color = axes->getDefaultYColor();
				Ogre::ColourValue z_color = axes->getDefaultZColor();
				x_color.a = base_alpha_property_->getFloat();
				y_color.a = base_alpha_property_->getFloat();
				z_color.a = base_alpha_property_->getFloat();
				axes->setXColor(x_color);
				axes->setYColor(y_color);
				axes->setZColor(z_color);
				axes->getSceneNode()->setVisible(true);
				axes->setScale(Ogre::Vector3(scale, scale, scale));
				base_axes_.push_back(axes);
			} else {
				// Adding the frame with a distant from the last one
				float sq_distant = xpos.squaredDistance(last_point_position_);
				if (sq_distant >= scale * scale * 0.0032) {
					Eigen::Quaterniond q = dwl::math::getQuaternion(rpy);
					quat.w = q.w();
					quat.x = q.x();
					quat.y = q.y();
					quat.z = q.z();

                                        boost::shared_ptr<rviz::Axes> axes;
					axes.reset(new Axes(scene_manager_, scene_node_, 0.04, 0.008));
					axes->setPosition(xpos);
					axes->setOrientation(quat * orientation); // combination of rotations
					Ogre::ColourValue x_color = axes->getDefaultXColor();
					Ogre::ColourValue y_color = axes->getDefaultYColor();
					Ogre::ColourValue z_color = axes->getDefaultZColor();
					x_color.a = base_alpha_property_->getFloat();
					y_color.a = base_alpha_property_->getFloat();
					z_color.a = base_alpha_property_->getFloat();
					axes->setXColor(x_color);
					axes->setYColor(y_color);
					axes->setZColor(z_color);
					axes->getSceneNode()->setVisible(true);
					axes->setScale(Ogre::Vector3(scale, scale, scale));
					base_axes_.push_back(axes);

					last_point_position_ = xpos;
				}
			}
		}

		base_manual_object_->end();
		break;
	}

	case BILLBOARDS: {
		// Getting the base line width
		float base_line_width = base_line_width_property_->getFloat();
		base_billboard_line_.reset(new rviz::BillboardLine(scene_manager_, scene_node_));
		base_billboard_line_->setNumLines(1);
		base_billboard_line_->setMaxPointsPerLine(num_points);
		base_billboard_line_->setLineWidth(base_line_width);

		for (uint32_t i = 0; i < num_points; ++i) {
			Ogre::Vector3 pos(0., 0., 0.);
			Ogre::Quaternion quat;
			Eigen::Vector3d rpy(0., 0., 0.);
			for (uint32_t j = 0; j < num_base; j++) {
				dwl_msgs::BaseState base = msg_->trajectory[i].base[j];
				if (base.id == dwl::rbd::LX)
					pos.x = base.position;
				else if (base.id == dwl::rbd::LY)
					pos.y = base.position;
				else if (base.id == dwl::rbd::LZ)
					pos.z = base.position;
				else if (base.id == dwl::rbd::AX)
					rpy(0) = base.position;
				else if (base.id == dwl::rbd::AY)
					rpy(1) = base.position;
				else
					rpy(2) = base.position;
			}

                        //sanity check position
                        if (!(std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z)))
                        {
                            std::cerr<<"whole body traj position is not finite, resetting to zero!" <<std::endl;
                            pos.x = 0.0;
                            pos.y = 0.0;
                            pos.z = 0.0;
                        }
                        //sanity check orientation
                        if (!(std::isfinite(rpy(0)) && std::isfinite(rpy(1)) && std::isfinite(rpy(2))))
                        {
                            std::cerr<<"whole body traj orientation is not finite, resetting to quat 1 0 0 0!" <<std::endl;
                            rpy(0) = 0.0;
                            rpy(1) = 0.0;
                            rpy(2) = 0.0;
                        }


			Ogre::Vector3 xpos = transform * pos;
			base_billboard_line_->addPoint(xpos, base_color);


			// We are keeping a vector of CoM frame pointers. This creates the next
			// one and stores it in the vector
			float scale = base_scale_property_->getFloat();
			if (i == 0 || i == num_points - 1) {
				last_point_position_ = xpos;

				// Adding the initial frame
				Eigen::Quaterniond q = dwl::math::getQuaternion(rpy);
				quat.w = q.w();
				quat.x = q.x();
				quat.y = q.y();
				quat.z = q.z();

				boost::shared_ptr<rviz::Axes> axes;
				axes.reset(new Axes(scene_manager_, scene_node_, 0.04, 0.008));
				axes->setPosition(xpos);
				axes->setOrientation(quat * orientation); // combination of rotations
				Ogre::ColourValue x_color = axes->getDefaultXColor();
				Ogre::ColourValue y_color = axes->getDefaultYColor();
				Ogre::ColourValue z_color = axes->getDefaultZColor();
				x_color.a = base_alpha_property_->getFloat();
				y_color.a = base_alpha_property_->getFloat();
				z_color.a = base_alpha_property_->getFloat();
				axes->setXColor(x_color);
				axes->setYColor(y_color);
				axes->setZColor(z_color);
				axes->getSceneNode()->setVisible(true);
				axes->setScale(Ogre::Vector3(scale, scale, scale));
				base_axes_.push_back(axes);
			} else {
				// Adding the frame with a distant from the last one
				float sq_distant = xpos.squaredDistance(last_point_position_);
				if (sq_distant >= scale * scale * 0.0032) {
					Eigen::Quaterniond q = dwl::math::getQuaternion(rpy);
					quat.w = q.w();
					quat.x = q.x();
					quat.y = q.y();
					quat.z = q.z();

					boost::shared_ptr<rviz::Axes> axes;
					axes.reset(new Axes(scene_manager_, scene_node_, 0.04, 0.008));
					axes->setPosition(xpos);
					axes->setOrientation(quat * orientation); // combination of rotations
					Ogre::ColourValue x_color = axes->getDefaultXColor();
					Ogre::ColourValue y_color = axes->getDefaultYColor();
					Ogre::ColourValue z_color = axes->getDefaultZColor();
					x_color.a = base_alpha_property_->getFloat();
					y_color.a = base_alpha_property_->getFloat();
					z_color.a = base_alpha_property_->getFloat();
					axes->setXColor(x_color);
					axes->setYColor(y_color);
					axes->setZColor(z_color);
					axes->getSceneNode()->setVisible(true);
					axes->setScale(Ogre::Vector3(scale, scale, scale));
					base_axes_.push_back(axes);

					last_point_position_ = xpos;
				}
			}
		}
		break;
	}

	case POINTS: {
		base_points_.clear();

		// Getting the base line width
		float base_line_width = base_line_width_property_->getFloat();
		for (uint32_t i = 0; i < num_points; ++i) {
			Ogre::Vector3 pos(0., 0., 0.);
			Ogre::Quaternion quat;
			Eigen::Vector3d rpy(0., 0., 0.);
			for (uint32_t j = 0; j < num_base; j++) {
				dwl_msgs::BaseState base = msg_->trajectory[i].base[j];
				if (base.id == dwl::rbd::LX)
					pos.x = base.position;
				else if (base.id == dwl::rbd::LY)
					pos.y = base.position;
				else if (base.id == dwl::rbd::LZ)
					pos.z = base.position;
				else if (base.id == dwl::rbd::AX)
					rpy(0) = base.position;
				else if (base.id == dwl::rbd::AY)
					rpy(1) = base.position;
				else
					rpy(2) = base.position;
			}

                        //sanity check position
                        if (!(std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z)))
                        {
                            std::cerr<<"whole body traj position is not finite, resetting to zero!" <<std::endl;
                            pos.x = 0.0;
                            pos.y = 0.0;
                            pos.z = 0.0;
                        }
                        //sanity check orientation
                        if (!(std::isfinite(rpy(0)) && std::isfinite(rpy(1)) && std::isfinite(rpy(2))))
                        {
                            std::cerr<<"whole body traj orientation is not finite, resetting to quat 1 0 0 0!" <<std::endl;
                            rpy(0) = 0.0;
                            rpy(1) = 0.0;
                            rpy(2) = 0.0;
                        }

			// We are keeping a vector of CoM visual pointers. This creates the next
			// one and stores it in the vector
			boost::shared_ptr<PointVisual> point_visual;
			point_visual.reset(new PointVisual(context_->getSceneManager(), scene_node_));
			point_visual->setColor(base_color.r, base_color.g, base_color.b, base_color.a);
			point_visual->setRadius(base_line_width);
			point_visual->setPoint(pos);
			point_visual->setFramePosition(position);
			point_visual->setFrameOrientation(orientation);

			// And send it to the end of the vector
			base_points_.push_back(point_visual);


			// We are keeping a vector of CoM frame pointers. This creates the next
			// one and stores it in the vector
			Ogre::Vector3 xpos = transform * pos;
			float scale = base_scale_property_->getFloat();
			if (i == 0 || i == num_points - 1) {
				last_point_position_ = xpos;

				// Adding the initial frame
				Eigen::Quaterniond q = dwl::math::getQuaternion(rpy);
				quat.w = q.w();
				quat.x = q.x();
				quat.y = q.y();
				quat.z = q.z();

				boost::shared_ptr<rviz::Axes> axes;
				axes.reset(new Axes(scene_manager_, scene_node_, 0.04, 0.008));
				axes->setPosition(xpos);
				axes->setOrientation(quat * orientation); // combination of rotations
				Ogre::ColourValue x_color = axes->getDefaultXColor();
				Ogre::ColourValue y_color = axes->getDefaultYColor();
				Ogre::ColourValue z_color = axes->getDefaultZColor();
				x_color.a = base_alpha_property_->getFloat();
				y_color.a = base_alpha_property_->getFloat();
				z_color.a = base_alpha_property_->getFloat();
				axes->setXColor(x_color);
				axes->setYColor(y_color);
				axes->setZColor(z_color);
				axes->getSceneNode()->setVisible(true);
				axes->setScale(Ogre::Vector3(scale, scale, scale));
				base_axes_.push_back(axes);
			} else {
				// Adding the frame with a distant from the last one
				float sq_distant = xpos.squaredDistance(last_point_position_);
				if (sq_distant >= scale * scale * 0.0032) {
					Eigen::Quaterniond q = dwl::math::getQuaternion(rpy);
					quat.w = q.w();
					quat.x = q.x();
					quat.y = q.y();
					quat.z = q.z();

					boost::shared_ptr<rviz::Axes> axes;
					axes.reset(new Axes(scene_manager_, scene_node_, 0.04, 0.008));
					axes->setPosition(xpos);
					axes->setOrientation(quat * orientation); // combination of rotations
					Ogre::ColourValue x_color = axes->getDefaultXColor();
					Ogre::ColourValue y_color = axes->getDefaultYColor();
					Ogre::ColourValue z_color = axes->getDefaultZColor();
					x_color.a = base_alpha_property_->getFloat();
					y_color.a = base_alpha_property_->getFloat();
					z_color.a = base_alpha_property_->getFloat();
					axes->setXColor(x_color);
					axes->setYColor(y_color);
					axes->setZColor(z_color);
					axes->getSceneNode()->setVisible(true);
					axes->setScale(Ogre::Vector3(scale, scale, scale));
					base_axes_.push_back(axes);

					last_point_position_ = xpos;
				}
			}
		}

		break;
	}
	}
}


void WholeBodyTrajectoryDisplay::processContactTrajectory()
{
	// Lookup transform into fixed frame
	Ogre::Vector3 position;
	Ogre::Quaternion orientation;
	if (!context_->getFrameManager()->getTransform(msg_->header, position, orientation)) {
		ROS_DEBUG("Error transforming from frame '%s' to frame '%s'",
				  msg_->header.frame_id.c_str(), qPrintable(fixed_frame_));
	}
	Ogre::Matrix4 transform(orientation);
	transform.setTrans(position);

	// Visualization of the end-effector trajectory
	// Getting the end-effector trajectory style
	uint32_t num_points = msg_->trajectory.size();
	uint32_t num_base = msg_->trajectory[0].base.size();
	LineStyle contact_style = (LineStyle) contact_style_property_->getOptionInt();

	// Getting the end-effector trajectory color
	Ogre::ColourValue contact_color = contact_color_property_->getOgreColor();
	contact_color.a = contact_alpha_property_->getFloat();

	// Getting the number of contact trajectories
	uint32_t num_traj = 0;
	std::map<std::string, uint32_t> contact_traj_id;
	for (uint32_t i = 0; i < num_points; ++i) {
		uint32_t num_contacts = msg_->trajectory[i].contacts.size();
		for (uint32_t k = 0; k < num_contacts; k++) {
			dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[k];

			if (contact_traj_id.find(contact.name) == contact_traj_id.end()) {// a new swing trajectory
				contact_traj_id[contact.name] = num_traj;

				// Incrementing the counter (id) of swing trajectories
				num_traj++;
			}
		}
	}

	// Visualizing the different end-effector trajectories
	contact_traj_id.clear();
	std::map<uint32_t, uint32_t> contact_vec_id;
	switch (contact_style)
	{
	case LINES:	{
		contact_manual_object_.clear();
		contact_manual_object_.resize(num_traj);

		uint32_t traj_id = 0;
		for (uint32_t i = 0; i < num_points; ++i) {
			uint32_t num_contacts = msg_->trajectory[i].contacts.size();
			for (uint32_t k = 0; k < num_contacts; k++) {
				dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[k];

				if (contact_traj_id.find(contact.name) == contact_traj_id.end()) {// a new swing trajectory
					contact_traj_id[contact.name] = traj_id;
					contact_vec_id[traj_id] = k;

					contact_manual_object_[traj_id].reset(scene_manager_->createManualObject());
					contact_manual_object_[traj_id]->setDynamic(true);
					scene_node_->attachObject(contact_manual_object_[traj_id].get());

					contact_manual_object_[traj_id]->estimateVertexCount(num_points);
					contact_manual_object_[traj_id]->begin("BaseWhiteNoLighting",
															Ogre::RenderOperation::OT_LINE_STRIP);

					// Incrementing the counter (id) of swing trajectories
					traj_id++;
				} else {
					uint32_t swing_idx = contact_traj_id.find(contact.name)->second;

					if (k != contact_vec_id.find(swing_idx)->second) {// change the vector index
						contact_vec_id[swing_idx] = k;
					}
				}
			}

			// Computing the actual base position
			Ogre::Vector3 base_pos(0., 0., 0.);
			Eigen::Vector3d base_rpy;
			for (uint32_t j = 0; j < num_base; j++) {
				dwl_msgs::BaseState base = msg_->trajectory[i].base[j];
				if (base.id == dwl::rbd::LX)
					base_pos.x = base.position;
				else if (base.id == dwl::rbd::LY)
					base_pos.y = base.position;
				else if (base.id == dwl::rbd::LZ)
					base_pos.z = base.position;
				else if (base.id == dwl::rbd::AX)
					base_rpy(0) = base.position;
				else if (base.id == dwl::rbd::AY)
					base_rpy(1) = base.position;
				else
					base_rpy(2) = base.position;
			}



			// Computing the base to world transform
			Eigen::Quaterniond quat = dwl::math::getQuaternion(base_rpy);
			Ogre::Quaternion ogre_quat(quat.w(), quat.x(), quat.y(), quat.z());
			Ogre::Matrix4 base_to_world_tf(ogre_quat);

			// Adding the contact points for the current swing trajectories
			for (std::map<std::string,uint32_t>::iterator traj_it = contact_traj_id.begin();
					traj_it != contact_traj_id.end(); traj_it++) {
				uint32_t traj_id = traj_it->second;
				uint32_t id = contact_vec_id.find(traj_id)->second;

				if (id < num_contacts) {
					dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[id];

					Ogre::Vector3 xpos = base_pos +
							base_to_world_tf * Ogre::Vector3(contact.position.x,
															 contact.position.y,
															 contact.position.z);

                                        //sanity check orientation
                                        if (!(std::isfinite(xpos.x) && std::isfinite(xpos.y) && std::isfinite(xpos.z)))
                                        {
                                            std::cerr<<"whole body contact trajectory is not finite, resetting to zero!" <<std::endl;
                                            xpos.x = 0.0;
                                            xpos.y = 0.0;
                                            xpos.z = 0.0;
                                        }
                                        contact_manual_object_[traj_id]->position(xpos.x, xpos.y, xpos.z);
					contact_manual_object_[traj_id]->colour(contact_color);
				}
			}
		}
		// Ending the contact manual objects
		for (uint32_t i = 0; i < traj_id; i++)
			contact_manual_object_[i]->end();

		break;
	}

	case BILLBOARDS: {
		// Getting the end-effector line width
		float contact_line_width = contact_line_width_property_->getFloat();
		contact_billboard_line_.clear();
		contact_billboard_line_.resize(num_traj);

		uint32_t traj_id = 0;
		for (uint32_t i = 0; i < num_points; ++i) {
			uint32_t num_contacts = msg_->trajectory[i].contacts.size();
			for (uint32_t k = 0; k < num_contacts; k++) {
				dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[k];

				if (contact_traj_id.find(contact.name) == contact_traj_id.end()) {// a new swing trajectory
					contact_traj_id[contact.name] = traj_id;
					contact_vec_id[traj_id] = k;

					contact_billboard_line_[traj_id].reset(new rviz::BillboardLine(scene_manager_, scene_node_));
					contact_billboard_line_[traj_id]->setNumLines(1);
					contact_billboard_line_[traj_id]->setMaxPointsPerLine(num_points);
					contact_billboard_line_[traj_id]->setLineWidth(contact_line_width);

					// Incrementing the counter (id) of swing trajectories
					traj_id++;
				} else {
					uint32_t swing_idx = contact_traj_id.find(contact.name)->second;

					if (k != contact_vec_id.find(swing_idx)->second) {// change the vector index
						contact_vec_id[swing_idx] = k;
					}
				}
			}

			// Computing the actual base position
			Ogre::Vector3 base_pos(0., 0., 0.);
			Eigen::Vector3d base_rpy;
			for (uint32_t j = 0; j < num_base; j++) {
				dwl_msgs::BaseState base = msg_->trajectory[i].base[j];
				if (base.id == dwl::rbd::LX)
					base_pos.x = base.position;
				else if (base.id == dwl::rbd::LY)
					base_pos.y = base.position;
				else if (base.id == dwl::rbd::LZ)
					base_pos.z = base.position;
				else if (base.id == dwl::rbd::AX)
					base_rpy(0) = base.position;
				else if (base.id == dwl::rbd::AY)
					base_rpy(1) = base.position;
				else
					base_rpy(2) = base.position;
			}

			// Computing the base to world transform
			Eigen::Quaterniond quat = dwl::math::getQuaternion(base_rpy);
			Ogre::Quaternion ogre_quat(quat.w(), quat.x(), quat.y(), quat.z());
			Ogre::Matrix4 base_to_world_tf(ogre_quat);

			// Adding the contact points for the current swing trajectories
			for (std::map<std::string,uint32_t>::iterator traj_it = contact_traj_id.begin();
					traj_it != contact_traj_id.end(); traj_it++) {
				uint32_t traj_id = traj_it->second;
				uint32_t id = contact_vec_id.find(traj_id)->second;

				if (id < num_contacts) {
					dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[id];

					Ogre::Vector3 xpos = base_pos +
							base_to_world_tf * Ogre::Vector3(contact.position.x,
															 contact.position.y,
															 contact.position.z);

                                        //sanity check orientation
                                        if (!(std::isfinite(xpos.x) && std::isfinite(xpos.y) && std::isfinite(xpos.z)))
                                        {
                                            std::cerr<<"whole body contact trajectory is not finite, resetting to zero!" <<std::endl;
                                            xpos.x = 0.0;
                                            xpos.y = 0.0;
                                            xpos.z = 0.0;
                                        }
                                        contact_billboard_line_[traj_id]->addPoint(xpos, contact_color);
				}
			}
		}
		break;
	}

	case POINTS: {
		// Getting the end-effector line width
		float contact_line_width = contact_line_width_property_->getFloat();
		contact_points_.clear();
		contact_points_.resize(num_points);

		uint32_t traj_id = 0;
		for (uint32_t i = 0; i < num_points; ++i) {
			uint32_t num_contacts = msg_->trajectory[i].contacts.size();
			for (uint32_t k = 0; k < num_contacts; k++) {
				dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[k];
				if (contact_traj_id.find(contact.name) == contact_traj_id.end()) {// a new swing trajectory
					contact_traj_id[contact.name] = traj_id;
					contact_vec_id[traj_id] = k;

					// Incrementing the counter (id) of swing trajectories
					traj_id++;
				} else {
					uint32_t swing_idx = contact_traj_id.find(contact.name)->second;

					if (k != contact_vec_id.find(swing_idx)->second) {// change the vector index
						contact_vec_id[swing_idx] = k;
					}
				}
			}

			// Updating the size
			contact_points_[i].clear();
			contact_points_[i].resize(num_contacts);

			// Computing the actual base position
			Ogre::Vector3 base_pos(0., 0., 0.);
			Eigen::Vector3d base_rpy;
			for (uint32_t j = 0; j < num_base; j++) {
				dwl_msgs::BaseState base = msg_->trajectory[i].base[j];
				if (base.id == dwl::rbd::LX)
					base_pos.x = base.position;
				else if (base.id == dwl::rbd::LY)
					base_pos.y = base.position;
				else if (base.id == dwl::rbd::LZ)
					base_pos.z = base.position;
				else if (base.id == dwl::rbd::AX)
					base_rpy(0) = base.position;
				else if (base.id == dwl::rbd::AY)
					base_rpy(1) = base.position;
				else
					base_rpy(2) = base.position;
			}

			// Computing the base to world transform
			Eigen::Quaterniond quat = dwl::math::getQuaternion(base_rpy);
			Ogre::Quaternion ogre_quat(quat.w(), quat.x(), quat.y(), quat.z());
			Ogre::Matrix4 base_to_world_tf(ogre_quat);

			// Adding the contact points for the current swing trajectories
			uint32_t contact_idx = 0;
			for (std::map<std::string,uint32_t>::iterator traj_it = contact_traj_id.begin();
					traj_it != contact_traj_id.end(); traj_it++) {
				uint32_t traj_id = traj_it->second;
				uint32_t id = contact_vec_id.find(traj_id)->second;

				if (id < num_contacts) {
					dwl_msgs::ContactState contact = msg_->trajectory[i].contacts[id];

					Ogre::Vector3 xpos = base_pos +
							base_to_world_tf * Ogre::Vector3(contact.position.x,
															 contact.position.y,
															 contact.position.z);


                                        //sanity check orientation
                                        if (!(std::isfinite(xpos.x) && std::isfinite(xpos.y) && std::isfinite(xpos.z)))
                                        {
                                            std::cerr<<"whole body contact trajectory is not finite, resetting to zero!" <<std::endl;
                                            xpos.x = 0.0;
                                            xpos.y = 0.0;
                                            xpos.z = 0.0;
                                        }
                                        contact_points_[i][contact_idx].reset(new PointVisual(context_->getSceneManager(),
																  scene_node_));
					contact_points_[i][contact_idx]->setColor(contact_color.r,
													  contact_color.g,
													  contact_color.b,
													  contact_color.a);
					contact_points_[i][contact_idx]->setRadius(contact_line_width);
					contact_points_[i][contact_idx]->setPoint(xpos);
					contact_points_[i][contact_idx]->setFramePosition(position);
					contact_points_[i][contact_idx]->setFrameOrientation(orientation);

					++contact_idx;
				}
			}
		}
		break;
	}
	}
}


void WholeBodyTrajectoryDisplay::destroyObjects()
{
	base_manual_object_.reset();
	base_billboard_line_.reset();
	base_points_.clear();
	base_axes_.clear();
	contact_manual_object_.clear();
	contact_billboard_line_.clear();
	for (uint32_t i = 0; i < contact_points_.size(); i++)
		contact_points_[i].clear();
	contact_points_.clear();
}

} // namespace dwl_rviz_plugin

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(dwl_rviz_plugin::WholeBodyTrajectoryDisplay, rviz::Display)
