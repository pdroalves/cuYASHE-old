#include "polynomial.h"
#include "common.h"
#include <omp.h>
#include <assert.h>

ZZ Polynomial::CRTProduct = ZZ(1);
std::vector<long> Polynomial::CRTPrimes(0);
ZZ Polynomial::global_mod = ZZ(0);
Polynomial *(Polynomial::global_phi) = NULL;

void Polynomial::update_device_data(){
    if(this->get_device_updated())
      return;

    #ifdef VERBOSE
    std::cout << "Copying data to GPU." << std::endl;
    #endif

    cudaError_t result;
    this->ON_COPY = true;


    result = cudaMalloc((void**)&this->d_polyCRT,this->CRTSPACING*(this->polyCRT.size())*sizeof(long));
    #ifdef VERBOSE
    std::cout << "cudaMalloc:" << cudaGetErrorString(result) << " "<< this->CRTSPACING*(this->polyCRT.size())*sizeof(long) << " bytes" <<std::endl;
    #endif
    assert(result == cudaSuccess);


    result = cudaMemset((void*)this->d_polyCRT,0,this->CRTSPACING*(this->polyCRT.size())*sizeof(long));
    #ifdef VERBOSE
    std::cout << "cudaMemset:" << cudaGetErrorString(result) << std::endl;
    #endif
    assert(result == cudaSuccess);

    for(unsigned int i=0;i < this->polyCRT.size();i++){
        result = cudaMemcpyAsync(this->d_polyCRT+this->CRTSPACING*i, &(this->polyCRT[i][0]) , (this->polyCRT[i].size())*sizeof(long), cudaMemcpyHostToDevice,this->stream);

        #ifdef VERBOSE
        std::cout << "cudaMemcpyAsync" << i << ": " << cudaGetErrorString(result) << " "<<(this->polyCRT[i].size())*sizeof(long) << " bytes to position "<< this->CRTSPACING*i*sizeof(int) <<std::endl;
        #endif
        assert(result == cudaSuccess);
    }

    result = cudaDeviceSynchronize();
    #ifdef VERBOSE
    std::cout << cudaGetErrorString(result) << std::endl;
    #endif
    assert(result == cudaSuccess);
    this->ON_COPY = false;
    this->set_device_updated(true);
}

void Polynomial::update_host_data(){
    if(this->get_host_updated())
      return;

    cudaError_t result;

    if(this->polyCRT.size() != Polynomial::CRTPrimes.size())
      this->polyCRT.resize(Polynomial::CRTPrimes.size());

    for(unsigned int i=0;i < this->polyCRT.size();i++){
      if(this->polyCRT[i].size() != this->CRTSPACING)
        this->polyCRT[i].resize(this->CRTSPACING);

        result = cudaMemcpyAsync(&(this->polyCRT[i][0]) , this->d_polyCRT+this->CRTSPACING*i, this->CRTSPACING*sizeof(long), cudaMemcpyDeviceToHost,this->stream);
        #ifdef VERBOSE
        std::cout << "cudaMemCpy" << i << ": " << cudaGetErrorString(result) <<" "<<this->CRTSPACING*sizeof(long) << " bytes to position "<< this->CRTSPACING*i*sizeof(long) <<std::endl;
        #endif
        assert(result == cudaSuccess);
    }

    result = cudaDeviceSynchronize();
    assert(result == cudaSuccess);

    this->set_host_updated(true);
}

void Polynomial::crt(){
    // "The product of those prime numbers should be larger than the potentially largest coefficient of polynomial c, that we will obtain as a result of a computation for accurate recovery through ICRT." produtorio_{i=1}^n (pi) > n*q^2

    // if(this->CRTProduct == NULL or this->CRTPrimes == NULL){
    //     throw -1;
    // }
    std::vector<long> P = this->CRTPrimes;
    this->polyCRT.resize(P.size());

    // Extract the coefficients to a array of ZZs
    std::vector<ZZ> array = this->get_coeffs();

    // We pick each prime
    for(std::vector<long>::iterator iter_prime = P.begin(); iter_prime != P.end(); iter_prime++){
        int index = iter_prime - P.begin();//Debug

        // Apply mod at each coefficient
        std::vector<long> rep = this->polyCRT[index];
        rep.resize(array.size());
        for(std::vector<ZZ>::iterator iter = array.begin();iter != array.end();iter++){
          // std::cout << "Prime: " << *iter_prime << std::endl;
          int array_i = iter-array.begin();//Debug
          rep[array_i] = conv<long>(*iter % (*iter_prime));
          // std::cout << "rep : " << rep[array_i] << ", ";
        }

         polyCRT[index] = (rep);
    }

    this->set_host_updated(true);
    this->set_device_updated(false);
}

void Polynomial::icrt(){
  // Escapes, if possible
  if(this->get_host_updated()){
    return;
  }else{
      this->update_host_data();
  }
  
  std::vector<long> P = this->CRTPrimes;
  ZZ M = this->CRTProduct;

  Polynomial icrt(this->get_mod(),this->get_phi(),this->get_crt_spacing());
  for(unsigned int i = 0; i < this->polyCRT.size();i++){
    // Convert CRT representations to Polynomial
    // Polynomial xi(this->get_mod(),this->get_phi(),this->get_crt_spacing());
    Polynomial xi(this->get_mod(),this->get_phi(),this->get_crt_spacing());
    xi.set_coeffs(this->polyCRT[i]);

    ZZ pi = ZZ(P[i]);
    ZZ Mpi= M/pi;
    ZZ InvMpi = NTL::InvMod(Mpi%pi,pi);

    // Polynomial step = ((xi*InvMpi % pi)*Mpi) % M;
    Polynomial step(xi);

    step *= InvMpi;
    step %= pi;
    step *= Mpi;

    icrt.CPUAddition(&step);
  }
  icrt %= M;
  icrt %= this->get_mod();
  icrt %= Polynomial::global_phi;
  icrt.normalize();
  this->copy(icrt);
  this->set_host_updated(true);
  return;
}

void Polynomial::DivRem(Polynomial a,Polynomial b,Polynomial &quot,Polynomial &rem){
  // Returns x = a % b

  if(a.get_host_updated() == false && b.get_host_updated() == false){
    // Operates on GPU
    throw "DivRem for GPU not implemented yet";
  }else{
    if(!a.get_host_updated()){
        a.update_host_data();
        a.icrt();
    }

    if(!b.get_host_updated()){
        b.update_host_data();
        b.icrt();
    }

    if(check_special_rem_format(b)){
      #ifdef VERBOSE
      std::cout << "Rem in special mode."<<std::endl;
      #endif

      Polynomial lower_half;
      int half = b.deg()-1;
      for(int i = a.deg(); i > half; i--)
        quot.set_coeff(i-b.deg(),a.get_coeff(i));
      for(int i = half;i >= 0;i--)
        lower_half.set_coeff(i,a.get_coeff(i));
      lower_half.CPUSubtraction(&quot);
      rem = lower_half;
    }else{
      throw "DivRem: I don't know how to div this!";
    }
  }
}

int isPowerOfTwo (unsigned int x){
  return ((x != 0) && !(x & (x - 1)));
}

void Polynomial::BuildNthCyclotomic(Polynomial *phi,int n){

  for(int i =0; i <= phi->deg(); i++)
    phi->set_coeff(i,0);
  if(isPowerOfTwo(n)){
    #ifdef VERBOSE
    std::cout << n << " is power of 2" << std::endl;
    #endif
    phi->set_coeff(0,1);
    phi->set_coeff(n,1);
    return;
  }else{
    #ifdef VERBOSE
    std::cout << n << " is not power of 2" << std::endl;
    #endif

    std::vector<Polynomial> aux_phi( n+1);

    for (long i = 1; i <= n; i++) {
       Polynomial t;
       t.set_coeff(0,ZZ(1));

       for (long j = 1; j <= i-1; j++)
          if (i % j == 0)
             t *= aux_phi[j];

       Polynomial mono;
       mono.set_coeff(i,ZZ(1));
       aux_phi[i] = (mono - 1)/t;

      //  cout << aux_phi[i] << "\n";
    }
    *phi = aux_phi[n];
 }
}

Polynomial Polynomial::get_phi(){
  // Returns a copy of phi
  // std::cout << "getphi!" << std::endl;
  // if(this->phi == NULL){
  //   // std::cout << "Using global phi." << std::endl;
  //   return *(this->global_phi);
  // }
  // std::cout << "Using local phi: " << this->phi->to_string() << std::endl;
  // // return *(this->phi);
  // return *(this->phi);

  return *(Polynomial::global_phi);
}