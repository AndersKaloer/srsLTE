#
# Copyright 2013-2015 Software Radio Systems Limited
#
# This file is part of the srsLTE library.
#
# srsLTE is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as
# published by the Free Software Foundation, either version 3 of
# the License, or (at your option) any later version.
#
# srsLTE is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# A copy of the GNU Affero General Public License can be found in
# the LICENSE file in the top-level directory of this distribution
# and at http://www.gnu.org/licenses/.
#

########################################################################
# LAYER MAPPING TEST  
########################################################################

ADD_EXECUTABLE(layermap_test layermap_test.c)
TARGET_LINK_LIBRARIES(layermap_test srslte)

ADD_TEST(layermap_single layermap_test -n 1000 -m single -c 1 -l 1) 

ADD_TEST(layermap_diversity_2 layermap_test -n 1000 -m diversity -c 1 -l 2) 
ADD_TEST(layermap_diversity_4 layermap_test -n 1000 -m diversity -c 1 -l 4) 

ADD_TEST(layermap_multiplex_11 layermap_test -n 1000 -m multiplex -c 1 -l 1) 
ADD_TEST(layermap_multiplex_12 layermap_test -n 1000 -m multiplex -c 1 -l 2) 
ADD_TEST(layermap_multiplex_13 layermap_test -n 1002 -m multiplex -c 1 -l 3) 
ADD_TEST(layermap_multiplex_14 layermap_test -n 1000 -m multiplex -c 1 -l 4) 
ADD_TEST(layermap_multiplex_15 layermap_test -n 1000 -m multiplex -c 1 -l 5) 
ADD_TEST(layermap_multiplex_16 layermap_test -n 1002 -m multiplex -c 1 -l 6) 
ADD_TEST(layermap_multiplex_17 layermap_test -n 994 -m multiplex -c 1 -l 7) 
ADD_TEST(layermap_multiplex_18 layermap_test -n 1000 -m multiplex -c 1 -l 8) 


ADD_TEST(layermap_multiplex_22 layermap_test -n 1000 -m multiplex -c 2 -l 2) 
ADD_TEST(layermap_multiplex_23 layermap_test -n 1002 -m multiplex -c 2 -l 3) 
ADD_TEST(layermap_multiplex_24 layermap_test -n 1000 -m multiplex -c 2 -l 4)
ADD_TEST(layermap_multiplex_25 layermap_test -n 1002 -m multiplex -c 2 -l 5)
ADD_TEST(layermap_multiplex_26 layermap_test -n 1002 -m multiplex -c 2 -l 6)
ADD_TEST(layermap_multiplex_27 layermap_test -n 1000 -m multiplex -c 2 -l 7)
ADD_TEST(layermap_multiplex_28 layermap_test -n 1000 -m multiplex -c 2 -l 8) 
 
 
########################################################################
# LAYER MAPPING TEST  
########################################################################
 
ADD_EXECUTABLE(precoding_test precoding_test.c)
TARGET_LINK_LIBRARIES(precoding_test srslte)

ADD_TEST(precoding_single precoding_test -n 1000 -m single) 
ADD_TEST(precoding_diversity2 precoding_test -n 1000 -m diversity -l 2 -p 2) 
ADD_TEST(precoding_diversity4 precoding_test -n 1024 -m diversity -l 4 -p 4) 



 


