// Example: project list operations (project_list)
// - Request topic : lixel/application/request/project_list   (ProjectListRequest)
// - Response topic: lixel/application/response/project_list  (ProjectListResponse)
// action supports:
//   AC_LIST_ALL_PROJECTS      list all projects (the default)
//   AC_MODIFY_PROJECT_LABELS  modify project labels (needs id and tag_name)
//   AC_DELETE_PROJECTS        delete projects (needs id_list)
// This example sends only the action; set req.set_id(), req.set_tag_name() or
// req.add_id_list() below for the last two.
// The same as python/example/usage/project_list.py.
#include "common.h"
#include "openapi/aiot_msgs.pb.h"

namespace pbm = ::openapi::aiot_msgs;
using lixel_examples::Args;

int main(int argc, char** argv) {
  Args args("project list operations");
  args.AddOption("action", "AC_LIST_ALL_PROJECTS",
                 "AC_LIST_ALL_PROJECTS / AC_MODIFY_PROJECT_LABELS / AC_DELETE_PROJECTS");
  args.Parse(argc, argv);

  pbm::ProjectListRequest req;
  pbm::ProjectListResponse rsp;
  req.set_action(lixel_examples::EnumOption<pbm::Application::ActionCmd>(
      args, "action", &pbm::Application_ActionCmd_Parse));
  return lixel_examples::RunRequest(args, "lixel/application/request/project_list", req,
                                    "lixel/application/response/project_list", &rsp,
                                    "ProjectListResponse");
}
