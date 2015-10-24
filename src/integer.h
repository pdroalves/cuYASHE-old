#ifndef INTEGER_H
#define INTEGER_H

#include <NTL/ZZ.h>
#include "polynomial.h"

class Integer{
	public:
		Integer(ZZ a){
			this->set_value(a);

			this->set_host_updated(true);
			this->set_device_updated(false);
			this->set_crt_computed(false);
			this->set_icrt_computed(true);
		}
		Integer(cuyasheint_t a){
			this->set_value(a);

			this->set_host_updated(true);
			this->set_device_updated(false);
			this->set_crt_computed(false);
			this->set_icrt_computed(true);
		}
		Integer operator=(ZZ a){			
			this->set_value(a);

			this->set_host_updated(true);
			this->set_device_updated(false);
			this->set_crt_computed(false);
			this->set_icrt_computed(true);

			return *this;
		}
		Integer operator=(cuyasheint_t a){			
			this->set_value(a);

			this->set_host_updated(true);
			this->set_device_updated(false);
			this->set_crt_computed(false);
			this->set_icrt_computed(true);

			return *this;
		}
		Integer operator/=(Integer a){			
			if(!this->get_host_updated())
				this->update_host_data();
			
			this->set_device_updated(false);
			return value / a.get_value();;
		}
		ZZ operator+(ZZ a){
			return value+a;
		}
		ZZ operator*(ZZ a){
			return value*a;
		}
		ZZ operator-(ZZ a){
			return value-a;
		}
		ZZ operator-(cuyasheint_t a){
			return value-a;
		}
		ZZ operator/(ZZ a){
			if(!this->get_host_updated())
				this->update_host_data();
			
			this->set_device_updated(false);
			value /= a;
			return value;
		}
		ZZ operator%(ZZ a){
			if(!this->get_host_updated())
				this->update_host_data();
			
			this->set_device_updated(false);
			value %= a;
			return value;
		}
		Polynomial operator+(Polynomial &a);
		Polynomial operator*(Polynomial &a);
		void set_value(ZZ a){
			value = a;

			this->set_host_updated(true);
		}
		void set_value(cuyasheint_t a){
			value = to_ZZ(a);

			this->set_host_updated(true);
		}
		ZZ get_value(){
			if(!this->get_host_updated())
				this->update_host_data();
			return value;
		}
		void set_host_updated(bool b){
	      this->HOST_IS_UPDATED = b;
	      if(b){        
	        #ifdef VERBOSE
	        std::cout << "Host data is updated" << std::endl;
	        #endif
	      }else{        
	        this->set_icrt_computed(false);
	        #ifdef VERBOSE
	        std::cout << "Host data is NOT updated" << std::endl;
	        #endif
	      }
	    }
	    bool get_host_updated(){

	      bool b = this->HOST_IS_UPDATED;
	      if(b){        
	        #ifdef VERBOSE
	        std::cout << "Host data is updated" << std::endl;
	        #endif
	      }else{        
	        this->set_icrt_computed(false);
	        #ifdef VERBOSE
	        std::cout << "Host data is NOT updated" << std::endl;
	        #endif
	      }

	      return b;
	    }
	    void set_device_updated(bool b){
	      this->DEVICE_IS_UPDATE = b;
	      if(b){
	        #ifdef VERBOSE
	        std::cout << "Device data is updated" << std::endl;
	        #endif
	      }
	      if(!b){
	        #ifdef VERBOSE
	        std::cout << "Device data is NOT updated" << std::endl;
	        #endif
	        this->set_crt_computed(false);
	      }
	    }
	    bool get_device_updated(){
	      bool b = this->DEVICE_IS_UPDATE;
	      if(b){        
	        #ifdef VERBOSE
	        std::cout << "Device data is updated" << std::endl;
	        #endif
	      }else{        
	        this->set_icrt_computed(false);
	        #ifdef VERBOSE
	        std::cout << "Device data is NOT updated" << std::endl;
	        #endif
	      }

	      return b;
	    }
	    void set_crt_computed(bool b){
	      this->CRT_COMPUTED = b;
	      if(b){        
	        #ifdef VERBOSE
	        std::cout << "CRT residues computed" << std::endl;
	        #endif
	      }else{        
	        #ifdef VERBOSE
	        std::cout << "CRT residues NOT computed" << std::endl;
	        #endif
	      }
	    }
	    bool get_crt_computed(){
	      return this->CRT_COMPUTED;
	    }
	    void set_icrt_computed(bool b){
	      this->ICRT_COMPUTED = b;

	      if(b){        
	        #ifdef VERBOSE
	        std::cout << "ICRT residues computed" << std::endl;
	        #endif
	      }else{        
	        #ifdef VERBOSE
	        std::cout << "ICRT residues NOT computed" << std::endl;
	        #endif
	      }
	    }
	    bool get_icrt_computed(){
	      return this->ICRT_COMPUTED;
	    }

		void crt(){
			// Escapes, if possible

			if(this->get_crt_computed())
			  return;
			else if(!this->get_host_updated())
			  throw "host is not updated";

			// Set the quantity of expected residues
			std::vector<cuyasheint_t> P = Polynomial::CRTPrimes;
			this->crt_values.resize(P.size());

			// We pick each prime
			#pragma omp parallel for  
			for(unsigned int i = 0; i < P.size();i++)
		    	crt_values[i] = conv<cuyasheint_t>(value % P[i]);
			  	

			this->set_device_updated(false);
			this->set_crt_computed(true);
		}
		void icrt();
		cuyasheint_t* get_device_crt_residues(){
			if(!this->get_device_updated())
				this->update_device_data();
			return this->d_crt_values;
		}
		void update_device_data();
		void update_host_data();
	private:
	    // Attributes
	    bool ON_COPY;
	    bool HOST_IS_UPDATED;
	    bool DEVICE_IS_UPDATE;
	    bool CRT_COMPUTED;
	    bool ICRT_COMPUTED;

		ZZ value;
		std::vector<cuyasheint_t> crt_values;
		cuyasheint_t *d_crt_values;
};

#endif