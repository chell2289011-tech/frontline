#include "sim.hpp"
#include <cstdio>
#include <cstdlib>
int main(int argc,char**argv){
  int days = argc>1?atoi(argv[1]):120; unsigned seed = argc>2?atoi(argv[2]):1;
  fl::World w; w.init(seed);
  auto st=w.stats();
  printf("start: UA cells %d RU cells %d total %d, brigades UA %d RU %d\n",st.cells[0],st.cells[1],st.total,st.brigades[0],st.brigades[1]);
  while(w.day<days && w.winner==fl::NONE_YET){ w.step();
    if(w.tick%(4*10)==0){ st=w.stats(); printf("day %3d: UA %4d RU %4d | br %2d/%2d men %6ld/%6ld lost %6ld/%6ld kyivDays %d\n",w.day,st.cells[0],st.cells[1],st.brigades[0],st.brigades[1],st.manpower[0],st.manpower[1],st.lost[0],st.lost[1],w.kyivDays);} }
  printf("winner %d day %d\n",w.winner,w.day);
}
