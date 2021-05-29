#include <ros/ros.h>
#include <pcl/common/transforms.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/common/transforms.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/passthrough.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/filters/extract_indices.h>
#include <pcl/io/pcd_io.h>
#include <unavlib/convt.h>
#include <unavlib/others.h>
#include <string>
#include <map>
#include <vector>

using namespace unavlib;

std::vector<int> dynamic_classes = {252, 253, 254, 255, 256, 257, 258, 259};

void parse_dynamic_obj(const pcl::PointCloud<pcl::PointXYZI>& cloudIn, pcl::PointCloud<pcl::PointXYZI>& dynamicOut, pcl::PointCloud<pcl::PointXYZI>& staticOut){
  dynamicOut.points.clear();
  staticOut.points.clear();

  for (const auto &pt: cloudIn.points){
    uint32_t float2int = static_cast<uint32_t>(pt.intensity);
    uint32_t semantic_label = float2int & 0xFFFF;
    uint32_t inst_label = float2int >> 16;
    bool is_static = true;
    for (int class_num: dynamic_classes){
      if (semantic_label == class_num){ // 1. check it is in the moving object classes
        dynamicOut.points.push_back(pt);
        is_static = false;
      }
    }
    if (is_static){
      staticOut.points.push_back(pt);
    }
  }
}
void calc_complement(pcl::PointCloud<pcl::PointXYZI>::Ptr src,
                     pcl::PointCloud<pcl::PointXYZI>::Ptr gt,
                          pcl::PointCloud<pcl::PointXYZI>& complement){
  if (!complement.empty()) complement.clear();
  //src: estimated map
  pcl::KdTreeFLANN<pcl::PointXYZI> kdtree;
  kdtree.setInputCloud(src);

  int K = 1;
  std::vector<int> pointIdxNKNSearch(K);
  std::vector<float> pointNKNSquaredDistance(K);

  for (const auto &pt: gt->points){
    if ( kdtree.nearestKSearch (pt, K, pointIdxNKNSearch, pointNKNSquaredDistance) > 0 )
    {
        uint32_t float2int = static_cast<uint32_t>(pt.intensity);
        uint32_t semantic_label = float2int & 0xFFFF;
        uint32_t inst_label = float2int >> 16;
        bool is_static = true;

        for (int class_num: dynamic_classes){
          if (semantic_label == class_num){ // 1. check it is in the moving object classes
            is_static = false;
          }
        }
        if (is_static && (pointNKNSquaredDistance[0] > 0.03)){
          complement.points.push_back(pt);
        }

     }
  }

}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "mapviz");
    std::cout<<"KiTTI MAPVIZ STARTED"<<std::endl;
    ros::NodeHandle nodeHandler;

    std::string rawName, octoMapName, pplName, removertName, erasorName;

    nodeHandler.param<std::string>("/raw", rawName, "/media/shapelim");
    nodeHandler.param<std::string>("/octoMap", octoMapName, "/media/shapelim");
    nodeHandler.param<std::string>("/pplremover", pplName, "/media/shapelim");
    nodeHandler.param<std::string>("/removert", removertName, "/media/shapelim");
    nodeHandler.param<std::string>("/erasor", erasorName, "/media/shapelim");


    ros::Publisher msPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/map/static", 100);
    ros::Publisher mdPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/map/dynamic", 100);

    ros::Publisher osPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/octomap/static", 100);
    ros::Publisher odPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/octomap/dynamic", 100);

    ros::Publisher psPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/pplremover/static", 100);
    ros::Publisher pdPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/pplremover/dynamic", 100);

    ros::Publisher rsPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/removert/static", 100);
    ros::Publisher rdPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/removert/dynamic", 100);

    ros::Publisher esPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/erasor/static", 100);
    ros::Publisher edPublisher = nodeHandler.advertise<sensor_msgs::PointCloud2>("/erasor/dynamic", 100);


    ////////////////////////////////////////////////////////////////////
    std::cout<<"\033[1;32mLoading map..."<<std::endl;
    // load original src
    pcl::PointCloud<pcl::PointXYZI>::Ptr ptr_src(new pcl::PointCloud<pcl::PointXYZI>);
    datahandle3d::load_pcd(rawName, ptr_src);

    // Removert
    pcl::PointCloud<pcl::PointXYZI>::Ptr ptr_octo(new pcl::PointCloud<pcl::PointXYZI>);
    datahandle3d::load_pcd(octoMapName, ptr_octo);

    // pplremover
    pcl::PointCloud<pcl::PointXYZI>::Ptr ptr_ppl(new pcl::PointCloud<pcl::PointXYZI>);
    datahandle3d::load_pcd(pplName, ptr_ppl);

    // Removert
    pcl::PointCloud<pcl::PointXYZI>::Ptr ptr_removert(new pcl::PointCloud<pcl::PointXYZI>);
    datahandle3d::load_pcd(removertName, ptr_removert);

    // Erasor
    pcl::PointCloud<pcl::PointXYZI>::Ptr ptr_ours(new pcl::PointCloud<pcl::PointXYZI>);
    datahandle3d::load_pcd(erasorName, ptr_ours);
    std::cout<<"\033[1;32mLoad complete \033[0m"<<std::endl;

    ////////////////////////////////////////////////////////////////////
    pcl::PointCloud<pcl::PointXYZI> mapStatic, mapDynamic;
    pcl::PointCloud<pcl::PointXYZI> octoStatic, octoDynamic;
    pcl::PointCloud<pcl::PointXYZI> pplStatic, pplDynamic;
    pcl::PointCloud<pcl::PointXYZI> removertStatic, removertDynamic;
    pcl::PointCloud<pcl::PointXYZI> erasorStatic, erasorDynamic;

    calc_complement(ptr_octo, ptr_src, octoStatic);
    calc_complement(ptr_ppl, ptr_src, pplStatic);
    calc_complement(ptr_removert, ptr_src, removertStatic);
    calc_complement(ptr_ours, ptr_src, erasorStatic);

    auto esmsg = erasor_utilscloud2msg(erasorStatic);
//    auto edmsg = erasor_utilscloud2msg(erasorDynamic);
    auto rsmsg = erasor_utilscloud2msg(removertStatic);
//    auto rdmsg = erasor_utilscloud2msg(removertDynamic);
    auto psmsg = erasor_utilscloud2msg(pplStatic);
//    auto pdmsg = erasor_utilscloud2msg(pplDynamic);
    auto osmsg = erasor_utilscloud2msg(octoStatic);
//    auto odmsg = erasor_utilscloud2msg(octoDynamic);
//    auto msmsg = erasor_utilscloud2msg(mapStatic);
//    auto mdmsg = erasor_utilscloud2msg(mapDynamic);

    ros::Rate loop_rate(2);
    static int count_ = 0;
    while (ros::ok())
    {
      esPublisher.publish(esmsg);
      rsPublisher.publish(rsmsg);
      psPublisher.publish(psmsg);
      osPublisher.publish(osmsg);


      if (++count_ %2 == 0) std::cout<<"On "<<count_<<"th publish!"<<std::endl;

      ros::spinOnce();
      loop_rate.sleep();
    }
    return 0;
}
