#ifdef NDEBUG
#error Tests require assertions; compile with -UNDEBUG
#endif
#include "mpccv/scene.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
int main(int argc,char** argv) {
    assert(argc==3);
    std::ifstream input(argv[1],std::ios::binary);assert(input);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
    mpccv::Scene scene;assert(mpccv::load_scene(data.data(),data.size(),scene));
    mpccv::Graph graph;assert(graph.configure(scene)==mpccv::GraphError::none);
    graph.armed=true;
    for(unsigned i=0;i<4410;++i) {
        mpccv::Transport time{true,true,double(i)*2/44100,120,44100};
        for(auto value:graph.sample(time,i).values)assert(value<=16383);
    }
    mpccv::SceneBytes bytes;assert(mpccv::save_scene(scene,bytes));
    std::ofstream output(argv[2],std::ios::binary);assert(output);
    output.write(reinterpret_cast<const char*>(bytes.bytes.data()),std::streamsize(bytes.size));assert(output);
}
