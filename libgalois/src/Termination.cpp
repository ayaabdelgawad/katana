/** Dikstra style termination detection -*- C++ -*-
 * @file
 * @section License
 *
 * This file is part of Galois.  Galois is a framework to exploit
 * amorphous data-parallelism in irregular programs.
 *
 * Galois is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, version 2.1 of the
 * License.
 *
 * Galois is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Galois.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * @section Copyright
 *
 * Copyright (C) 2015, The University of Texas at Austin. All rights
 * reserved.
 *
 * @section Description
 *
 * Implementation of Dikstra dual-ring Termination Detection
 *
 * @author Andrew Lenharth <andrew@lenharth.org>
 */

#include "Galois/gIO.h"
#include "Galois/Substrate/Termination.h"

// vtable anchoring
galois::substrate::TerminationDetection::~TerminationDetection(void) {}

static galois::substrate::TerminationDetection* TERM = nullptr;

void galois::substrate::internal::setTermDetect(galois::substrate::TerminationDetection* t) {
  GALOIS_ASSERT(!(TERM && t), "Double initialization of TerminationDetection");
  TERM = t;
}


galois::substrate::TerminationDetection& galois::substrate::getSystemTermination(unsigned activeThreads) {
  TERM->init(activeThreads);
  return *TERM;
}
