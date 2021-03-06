#include "main.h"

//TODO: Multiblock prefix sum scan implementation (especially for compact)
//TODO: One global memory fetch only! in kernel_ccl_mesh_B_improved
//TODO: Label equivalence algorithm for connected components labeling


// helper functions and utilities to work with CUDA
#include "helper_cuda.h"
#include "helper_functions.h"

#define NUM_BANKS 32		//16 for compute capability < 2, 32 for compute capability >= 2
#define LOG_NUM_BANKS 5		//4 for compute capability < 2, 5 for compute capability >= 2
#ifdef ZERO_BANK_CONFLICTS 
#define CONFLICT_FREE_OFFSET(n) \ 
 ((n) >> NUM_BANKS + (n) >> (2 * LOG_NUM_BANKS)) 
#else 
#define CONFLICT_FREE_OFFSET(n) ((n) >> LOG_NUM_BANKS) 
#endif

float4 *d_data;
unsigned int *d_hists;
float *d_vars;
unsigned int *d_ghist;
unsigned int *d_cdf; 
unsigned int *d_max;
unsigned int *d_max_old;
unsigned int *d_max_gid;
unsigned int *d_trbuffer;
unsigned int *d_trremaining;
unsigned int *d_trinds;
uint4 *d_min;
unsigned int *d_trlabels;
unsigned int *d_trsize;
unsigned int *d_trcompact;
unsigned int *d_ntr; 
unsigned int *d_classified;
unsigned int *d_roadlabel;
float *d_means;
float *d_invCovs;
float *d_weights;
float *d_probs;
float *d_n;
float *d_means_new;
float *d_invCovs_new;
float *d_weights_new;
float *d_probs_new;
float *d_n_new;
unsigned int *d_ind_clusters_min_weights;
unsigned int *d_irand;
unsigned int *m_d_IsNotDone;

// 1D float4 texture for r,g,b and y data
texture<float4, cudaTextureType1D, cudaReadModeElementType> texImg;
// 1D unsigned int texture for r,g,b cdfs
texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> texCdf;
// 1D unsigned char texture for training patch location lookup table
//texture<unsigned char, cudaTextureType1D, cudaReadModeElementType> texTrBinary;
// 1D unsigned int texture for training patch location lookup table
texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> texTrLabels;
// 1D unsigned int texture for training patch location lookup table
texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> texTrSize;
// 1D unsigned int texture for training patch compact indices
texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> texTrCompact;
// 1D unsigned int texture for classification
texture<unsigned int, cudaTextureType1D, cudaReadModeElementType> texClassified;

//Memory sizes
unsigned int mem_size_data;
unsigned int mem_size_hists;
unsigned int mem_size_vars;
unsigned int mem_size_ghist;
unsigned int mem_size_cdf;
unsigned int mem_size_trlabels;
unsigned int mem_size_trsize;
unsigned int mem_size_trcompact;
unsigned int mem_size_classified;
unsigned int mem_size_means;
unsigned int mem_size_invCovs;
unsigned int mem_size_weights;
unsigned int mem_size_probs;
unsigned int mem_size_n;
unsigned int mem_size_means_new;
unsigned int mem_size_invCovs_new;
unsigned int mem_size_weights_new;
unsigned int mem_size_probs_new;
unsigned int mem_size_n_new;
unsigned int mem_size_irand;


///////////////////////// CLASSIFICATION ///////////////////////////

// Filter the classified patch that is connected to the training samples
__global__ void kernel_filter(unsigned int *d_trlabels, unsigned int *d_cond, long n_patches)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int cond = d_cond[0];

	while (gid < n_patches)
	{
		d_trlabels[gid] = d_trlabels[gid] == cond ? 1 : 0;
		gid += gridDim.x * blockDim.x;
	}
}

// Classification via Gaussian models and Mahalanobis distance threshold
__global__ void kernel_classification_raw(unsigned int *d_hists, unsigned int *d_classified, unsigned int *d_trlabels, float *d_means, float *d_invCovs, long size, unsigned int n_models, float threshold, float inf)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;

	while (gid < size)
	{
		//calculate the minimum mahalanobis distance between the sample and the model
		float mindist = inf;
		for (int i = 0; i < n_models; i++)
		{
			float sum = 0.0f;
			for (int j = 0; j < N_BINS_SQR; j++)
			{
				float diff = d_hists[j * size + gid] - d_means[i * N_BINS_SQR + j];
				sum += diff * d_invCovs[i * N_BINS_SQR + j] * diff;				
			}
			mindist = fminf(mindist, sum);
		}
		d_classified[gid] = (mindist < threshold) || d_trlabels[gid];
		gid += offset;
	}
}

////////////////// MODEL UPDATING /////////////////

// Number of blocks are the number of models to be replaced. 
// Number of threads are the dimension of feature space used.
__global__ void kernel_replace_models(float *d_means, float *d_invCovs, float *d_weights, float *d_means_new, float *d_invCovs_new, float *d_weights_new, unsigned int *d_ind_clusters_min_weights)
{
	unsigned int m = blockIdx.x;
	unsigned int d = threadIdx.x;
	unsigned int m_replaced = d_ind_clusters_min_weights[m];
	d_means[m_replaced * N_BINS_SQR + d] = d_means_new[m * N_BINS_SQR + d];
	d_invCovs[m_replaced * N_BINS_SQR + d] = d_invCovs_new[m * N_BINS_SQR + d];
	if (threadIdx.x == 0)
		d_weights[m_replaced] = d_weights_new[m];
}

__global__ void kernel_update_models_step2(float *d_means, float *d_invCovs, float *d_weights, unsigned int n_models, float cov_min, float inf)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	while (gid < N_BINS_SQR * n_models)
	{
		float weight = d_weights[gid/N_BINS_SQR];
		float invC = d_invCovs[gid];
		float dummycov = 0.0f;
		d_means[gid] = d_means[gid] / weight;
		if (invC > cov_min)
			dummycov = weight / invC;
		else
			dummycov = inf;
		d_invCovs[gid] = dummycov;
		gid += offset;
	}
}


__global__ void kernel_update_models_step1(unsigned int *d_trbuffer, unsigned int *d_trinds,  float *d_means, float *d_invCovs, float *d_weights, unsigned int n_models, float threshold, float inf, unsigned int *ntr)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int tid = threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	unsigned int n_tr = *ntr;
	unsigned int n_tr_removed = 0;
	__shared__ float sum_mean [N_BINS_SQR * N_CLUSTERS_MAX];
	__shared__ float sum_invCov [N_BINS_SQR * N_CLUSTERS_MAX];
	__shared__ float sum_weight [N_CLUSTERS_MAX];

	//initialize partial sums
	while (tid < N_BINS_SQR * n_models)
	{
		sum_mean[tid] = 0.0f;
		sum_invCov[tid] = 0.0f;
		sum_weight[tid/N_BINS_SQR] = 0.0f;
		tid += blockDim.x;
	}
	__syncthreads();

	while (gid < n_tr)
	{
		//calculate the minimum mahalanobis distance between the sample and the model
		float weight_sample = 1.0f/n_tr;
		float mindist = inf;
		unsigned int minind = 0;
		// find the minimum mahalanobis distance from the sample to the models
		// and the index of the closest model
		for (int i = 0; i < n_models; i++)
		{
			float sum = 0.0f;
			//calculate the mahalanobis distance to the model i
			for (int j = 0; j < N_BINS_SQR; j++)
			{
				float diff = d_trbuffer[j * n_tr + gid] - d_means[i * N_BINS_SQR + j];
				sum += diff * d_invCovs[i * N_BINS_SQR + j] * diff;				
			}
			minind = i * (sum < mindist);
			mindist = fminf(mindist, sum);
		}

		//if the minimum distance is smaller than the update threshold, 
		//calculate partial updates to the model
		if (mindist < threshold)
		{
			d_trinds[gid] = 0;
			for (int i = 0; i < N_BINS_SQR; i++)
			{
				float sample = d_trbuffer[i * n_tr + gid];
				float mean = d_means[minind * N_BINS_SQR + i];
				atomicAdd(&sum_mean[minind * N_BINS_SQR + i] ,weight_sample * sample);
				atomicAdd(&sum_invCov[minind * N_BINS_SQR + i] ,weight_sample * (sample - mean) * (sample - mean));
			}
			atomicAdd(&sum_weight[minind], weight_sample);
			n_tr_removed++;
		}
		gid += offset;
	}
	__syncthreads();
	atomicSub(&ntr[0], n_tr_removed);
	
	tid = threadIdx.x;
	//update all dimensions of all models
	while (tid < N_BINS_SQR * n_models && *ntr != n_tr)
	{
		if (blockIdx.x == 0)
		{
			float weight = d_weights[tid/N_BINS_SQR];
			float invC = d_invCovs[tid];
			d_means[tid] = d_means[tid] * weight;
			d_invCovs[tid] = (invC < inf - 5.0f) / invC * weight;		
		}
		atomicAdd(&d_means[tid], sum_mean[tid]);
		atomicAdd(&d_invCovs[tid], sum_invCov[tid]);
		atomicAdd(&d_weights[tid/N_BINS_SQR], sum_weight[tid/N_BINS_SQR]);
		tid += blockDim.x;
	}
}



////////////////// EXPECTATION MAXIMIZATION ///////////////////////

///Structure of Arrays
typedef struct
{
float* n; // expected # of events in cluster: [M]
float* pi; // probability of cluster in GMM: [M]
float* constant; // Pre-computed constant for E-step: [M]
float* means; // Spectral mean for the cluster: [M*D]
float* R; // Covariance matrix: [M*D*D]
float* Rinv; // Inverse of covariance matrix: [M*D*D]
float* memberships; // Fuzzy memberships: [M*N]
} clusters_t;

__global__ void kernel_maximization_step_logconsts(float *d_invCovs, float *d_logconsts)
{
	unsigned int m = blockIdx.x;
	unsigned int offset = blockDim.x;
	extern __shared__ float sums[];
	float sum = 0.0f;

	for(int i = threadIdx.x; i < N_BINS_SQR; i += offset)
	{
		sum += logf(d_invCovs[m*N_BINS_SQR+i]);
	}

	sums[threadIdx.x] = sum;
	__syncthreads();

	for (int s = blockDim.x>>1; s > 0; s >>= 1) // build sum in place up the tree 
	{ 
		if (threadIdx.x < s)
		{
			sums[threadIdx.x] += sums[threadIdx.x + s];	
		}
		__syncthreads();
	}

	if(threadIdx.x == 0)
		d_logconsts[m] = (-N_BINS_SQR/2.0f)*logf(2.0f*CUDART_PI_F) - (0.5f * sums[threadIdx.x]);
}


/// For a diagonal covariance matrix, this implementation is the same as the means implementation
__global__ void kernel_maximization_step_covariance(unsigned int *d_trbuffer, float *d_probs, float *d_means, float *d_invCovs, float *d_n, unsigned int *d_max, float inf, float cov_min)
{
	unsigned int m = blockIdx.x;
	unsigned int d = blockIdx.y;
	unsigned int offset = blockDim.x;
	unsigned int n_tr = *d_max;
	extern __shared__ float sums[];
	float sum = 0.0f;

	for(int i = threadIdx.x; i < n_tr; i += offset)
	{
		float mean = d_means[m*N_BINS_SQR+d];
		float tr = d_trbuffer[d*n_tr+i];
		sum += (tr-mean)*(tr-mean)*d_probs[m*n_tr+i];
	}

	sums[threadIdx.x] = sum;
	__syncthreads();

	for (int s = blockDim.x>>1; s > 0; s >>= 1) // build sum in place up the tree 
	{ 
		if (threadIdx.x < s)
		{
			sums[threadIdx.x] += sums[threadIdx.x + s];	
		}
		__syncthreads();
	}

	if(threadIdx.x == 0)
		d_invCovs[m*N_BINS_SQR+d] = (sums[threadIdx.x] < cov_min) ? inf : d_n[m]/sums[threadIdx.x];
}

__global__ void kernel_maximization_step_means(unsigned int *d_trbuffer, float *d_probs, float *d_means, float *d_n, unsigned int *d_max)
{
	unsigned int m = blockIdx.x;
	unsigned int d = blockIdx.y;
	unsigned int offset = blockDim.x;
	unsigned int n_tr = *d_max;
	extern __shared__ float sums[];
	float sum = 0.0f;

	for(int i = threadIdx.x; i < n_tr; i += offset)
		sum += d_trbuffer[d*n_tr+i]*d_probs[m*n_tr+i];

	sums[threadIdx.x] = sum;
	__syncthreads();

	for (int s = blockDim.x>>1; s > 0; s >>= 1) // build sum in place up the tree 
	{ 
		if (threadIdx.x < s)
		{
			sums[threadIdx.x] += sums[threadIdx.x + s];	
		}
		__syncthreads();
	}
	
	if(threadIdx.x == 0)
		d_means[m*N_BINS_SQR+d] = sums[threadIdx.x]/d_n[m];
}

__global__ void kernel_maximization_step_n(float *d_probs, float *d_n, float *d_weights, unsigned int *d_max)
{
	unsigned int m = blockIdx.x;
	unsigned int n_tr = *d_max;
	unsigned int offset = blockDim.x;
	extern __shared__ float sums[];
	float sum = 0.0f;

	for(int i = threadIdx.x; i < n_tr; i += offset)
		sum += d_probs[m*n_tr+i];

	sums[threadIdx.x] = sum;
	__syncthreads();

	for (int s = blockDim.x>>1; s > 0; s >>= 1) // build sum in place up the tree 
	{ 
		if (threadIdx.x < s)
		{
			sums[threadIdx.x] += sums[threadIdx.x + s];	
		}
		__syncthreads();
	}

	if(threadIdx.x == 0)
	{
		float dummy_n = sums[threadIdx.x];
		d_n[m] = dummy_n;
		d_weights[m] = dummy_n / n_tr;
	}
}

__global__ void kernel_expectation_step_2(float* d_probs, float* d_likelihood, unsigned int *d_max) {
	
	extern __shared__ float shared_likelihood[];
	shared_likelihood[threadIdx.x] = 0.0;
	
	// determine data indices based on block index, grid size
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	unsigned int n_tr = *d_max;
	
	while (gid < n_tr)
	{
		// find the maximum likelihood for this event
		float max = d_probs[gid];
		for(int c=1; c<N_CLUSTERS_INITIAL; c++)
			max = fmaxf(max,d_probs[c*n_tr+gid]);
		
		// Compute denominator (sum of weighted likelihoods)
		float denominator = 0;
		for(int c=0; c<N_CLUSTERS_INITIAL; c++)
			denominator += expf(d_probs[c*n_tr+gid]-max);
		denominator = max + logf(denominator);
		shared_likelihood[threadIdx.x] += denominator;
		
		// Divide by denominator and undo the log
		for(int c=0; c<N_CLUSTERS_INITIAL; c++)
			d_probs[c*n_tr+gid] = expf(d_probs[c*n_tr+gid] - denominator);
		gid += offset;
	}
	__syncthreads();

	for (int s = blockDim.x>>1; s > 0; s >>= 1) // build sum in place up the tree 
	{ 
		if (threadIdx.x < s)
			shared_likelihood[threadIdx.x] += shared_likelihood[threadIdx.x + s];	
		__syncthreads();
	}

	if (threadIdx.x == 0)
		d_likelihood[blockIdx.x] = shared_likelihood[threadIdx.x];
}

/////// From Thesis: Scalable Data Clustering using GPUs
/////// Diagonal Cov Matrix Implementation
__global__ void kernel_expectation_step_1(unsigned int *d_trbuffer, float *d_means, float *d_invCovs, float *d_weights, float *d_probs, unsigned int *d_irand, unsigned int *d_max, bool isFirstIt)
{
	//cluster id
	unsigned int cid = blockIdx.y;	
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int n_tr = d_max[0];
	unsigned int offset = gridDim.x * blockDim.x;
	//float log_const;
	float log_weight;

	__shared__ float s_mean[N_BINS_SQR];
	__shared__ float s_invCov[N_BINS_SQR];

	if (isFirstIt)
		log_weight = logf(1.0f/gridDim.y);
	else
		log_weight = logf(d_weights[cid]);

	if (threadIdx.x == 0)
	{
		for (int i = 0; i < N_BINS_SQR; i++)
		{
			if (isFirstIt)
			{
				s_mean[i] = d_trbuffer[i * n_tr + d_irand[cid]];
				s_invCov[i] = 1;
			}
			else
			{
				s_mean[i] = d_means[cid * N_BINS_SQR + i];
				s_invCov[i] = d_invCovs[cid * N_BINS_SQR + i];
			}
		}
	}
	__syncthreads();

	while (gid < n_tr)
	{
		float LL = 0.0f;
		for (int i = 0; i < N_BINS_SQR; i++)
		{
			float data = d_trbuffer[i * n_tr + gid];
			LL += (data - s_mean[i]) * (data - s_mean[i]) * s_invCov[i];
		}
		d_probs[cid * n_tr + gid] = -0.5f * LL + log_weight;
		gid += offset;
	}
}

////////////////// TRAINING BUFFER ALLOCATION AND ASSIGNING ////////////

/// TODO: Try d_max as an array of the same value with size same as the downsampled image 
__global__ void kernel_insert_remaining_training_samples(unsigned int *d_trlabels, unsigned int *d_trcompact, unsigned int *d_trbuffer, unsigned int *d_trbuffer_remaining, unsigned int *d_max, unsigned int *d_max_old, long n_patches)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	unsigned int n_tr = d_max[0];
	unsigned int n_tr_old = d_max_old[0];

	while (gid < n_patches)
	{
		if (d_trlabels[gid] != 0)
		{
			unsigned int id = d_trcompact[gid];
			for (int i = 0; i < N_BINS_SQR; i++)
				d_trbuffer_remaining[i * n_tr + id] = d_trbuffer[i * n_tr_old + gid];
		}

		gid += offset;
	}
}


/// TODO: Try d_max as an array of the same value with size same as the downsampled image 
__global__ void kernel_insert_training_samples_w_inds(unsigned int *d_trlabels, unsigned int *d_trcompact, unsigned int *d_hists, unsigned int *d_trbuffer, unsigned int *d_trinds, unsigned int *d_max, long n_patches)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	unsigned int n_tr = d_max[0];

	while (gid < n_patches)
	{
		if (d_trlabels[gid] != 0)
		{
			unsigned int id = d_trcompact[gid];
			for (int i = 0; i < N_BINS_SQR; i++)
				d_trbuffer[i * n_tr + id] = d_hists[i * n_patches + gid];
			d_trinds[id] = 1;
		}
		gid += offset;
	}
}

////////////////// CONNECTED COMPONENTS LABELING //////////////////

__global__ void kernel_filterbiggestblob(unsigned int *d_trlabels, unsigned int *d_max_gid, long n_patches)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int max = d_max_gid[0];

	while (gid < n_patches)
	{
		d_trlabels[gid] = d_trlabels[gid] == max ? 1 : 0;
		gid += gridDim.x * blockDim.x;
	}
}

//TODO : Can Remove d_max 
//Blocksize should be a power of 2
__global__ void kernel_findmax(unsigned int *d_trsize, unsigned int *d_max, unsigned int *d_max_gid, long size_max)
{
	unsigned int gid = threadIdx.x;
	unsigned int max = 0;
	unsigned int max_gid = 0;
	__shared__ unsigned int s_max [512];
	__shared__ unsigned int s_max_gid [512];

	while (gid < size_max)
	{
		s_max[threadIdx.x] = d_trsize[gid];
		s_max_gid[threadIdx.x] = gid;

		for (int s = blockDim.x / 2; s > 0; s >>= 1)
		{
			if (threadIdx.x < s)
			{
				unsigned int dum1 = s_max[threadIdx.x];
				unsigned int dum2 = s_max[threadIdx.x + s];

				if (dum1 < dum2)
				{
					s_max[threadIdx.x] = dum2;
					s_max_gid[threadIdx.x] = s_max_gid[threadIdx.x + s];
				}
			}
			__syncthreads();
		}
		
		if (threadIdx.x == 0 && s_max[threadIdx.x] > max)
		{
			max = s_max[threadIdx.x];
			max_gid = s_max_gid[threadIdx.x];
		}
		__syncthreads();

		gid += blockDim.x;
	}

	if (threadIdx.x == 0)
	{
		d_max[threadIdx.x] = max;
		d_max_gid[threadIdx.x] = max_gid;
	}
}

// TODO : One global memory fetch only!
// TODO : Remove size calculation
////////////// ADAPTED FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA (Mesh Kernel B) //////////////
__global__ void kernel_ccl_mesh_B_improved_classify(unsigned int *Ld, unsigned int *d_roadlabel, unsigned int *d_maxgid, unsigned int *md, int widthgrid, int heightgrid)
{
	int id = (blockIdx.y * blockDim.y + threadIdx.y) * widthgrid + blockIdx.x * blockDim.x + threadIdx.x;
	int idl = threadIdx.y * blockDim.x + threadIdx.x;
	int idn = 0;
	unsigned int label = tex1Dfetch(texClassified, id);
	unsigned int nid [4];

	extern __shared__ unsigned int Ll [];
	__shared__ unsigned int ml;
	ml = 1;

	////// GLOBAL PART ///////
	if (label)
	{
		//upper neighbour
		if ((id - widthgrid) >= 0 && label * tex1Dfetch(texClassified, id - widthgrid))
		{
			nid[idn] = tex1Dfetch(texClassified, id - widthgrid);
			idn++;
		}
		//lower neighbour
		if ((id + widthgrid) < widthgrid * heightgrid && label * tex1Dfetch(texClassified, id + widthgrid))
		{
			nid[idn] = tex1Dfetch(texClassified, id + widthgrid);
			idn++;
		}
		//left neighbour
		if (((id - 1) % widthgrid) != (widthgrid - 1) && label * tex1Dfetch(texClassified, id - 1))
		{
			nid[idn] = tex1Dfetch(texClassified, id - 1);
			idn++;
		}
		//right neighbour
		if (((id + 1) % widthgrid) != 0 && label * tex1Dfetch(texClassified, id + 1))
		{
			nid[idn] = tex1Dfetch(texClassified, id + 1);
			idn++;
		}

		for (int i = 0; i < idn; i++)
			if (nid[i] < label)
			{
				label = nid[i];
				md[0] = 1;
			}

	
		///// SHARED PART ///////
		idn = 0;
		//within block upper neighbour
		if ((int)(idl - blockDim.x) >= 0 && label * tex1Dfetch(texClassified, id - widthgrid))
		{
			nid[idn] = idl - blockDim.x;
			idn++;
		}
		//within block lower neighbour
		if ((int)(idl + blockDim.x) < blockDim.x * blockDim.y && label * tex1Dfetch(texClassified, id + widthgrid))
		{
			nid[idn] = idl + blockDim.x;
			idn++;
		}
		//within block left neighbour
		if ((int)((idl - 1) % blockDim.x) != (blockDim.x - 1) && label * tex1Dfetch(texClassified, id - 1))
		{
			nid[idn] = idl - 1;
			idn++;
		}
		//within block right neighbour
		if ((int)((idl + 1) % blockDim.x) != 0 && label * tex1Dfetch(texClassified, id + 1))
		{
			nid[idn] = idl + 1;
			idn++;
		}

		while (ml)
		{
			Ll[idl] = label;
			__syncthreads();
			ml = 0;
			
			
			for (int i = 0; i < idn; i++)
				if (Ll[nid[i]] < label)
				{
					label = Ll[nid[i]];
					ml = 1;
				}
			__syncthreads();
		}

		Ld[id] = label;
		if (id == d_maxgid[0])
			d_roadlabel[0] = label;
	}

}


// TODO : One global memory fetch only!
////////////// ADAPTED FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA (Mesh Kernel B) //////////////
__global__ void kernel_ccl_mesh_B_improved(unsigned int *d_trlabels, unsigned int *d_trsize, unsigned int *m_d_IsNotDone, int widthgrid, int heightgrid)
{
	int id = (blockIdx.y * blockDim.y + threadIdx.y) * widthgrid + blockIdx.x * blockDim.x + threadIdx.x;
	int idl = threadIdx.y * blockDim.x + threadIdx.x;
	int idn = 0;
	unsigned int label = tex1Dfetch(texTrLabels, id);
	unsigned int nid [4];

	extern __shared__ unsigned int Ll [];
	__shared__ unsigned int ml;
	ml = 1;

	////// GLOBAL PART ///////
	if (label)
	{
		//upper neighbour
		if ((id - widthgrid) >= 0 && label * tex1Dfetch(texTrLabels, id - widthgrid))
		{
			nid[idn] = tex1Dfetch(texTrLabels, id - widthgrid);
			idn++;
		}
		//lower neighbour
		if ((id + widthgrid) < widthgrid * heightgrid && label * tex1Dfetch(texTrLabels, id + widthgrid))
		{
			nid[idn] = tex1Dfetch(texTrLabels, id + widthgrid);
			idn++;
		}
		//left neighbour
		if (((id - 1) % widthgrid) != (widthgrid - 1) && label * tex1Dfetch(texTrLabels, id - 1))
		{
			nid[idn] = tex1Dfetch(texTrLabels, id - 1);
			idn++;
		}
		//right neighbour
		if (((id + 1) % widthgrid) != 0 && label * tex1Dfetch(texTrLabels, id + 1))
		{
			nid[idn] = tex1Dfetch(texTrLabels, id + 1);
			idn++;
		}

		for (int i = 0; i < idn; i++)
			if (nid[i] < label)
			{
				label = nid[i];
				m_d_IsNotDone[0] = 1;
			}

	
		///// SHARED PART ///////
		idn = 0;
		//within block upper neighbour
		if ((int)(idl - blockDim.x) >= 0 && label * tex1Dfetch(texTrLabels, id - widthgrid))
		{
			nid[idn] = idl - blockDim.x;
			idn++;
		}
		//within block lower neighbour
		if ((int)(idl + blockDim.x) < blockDim.x * blockDim.y && label * tex1Dfetch(texTrLabels, id + widthgrid))
		{
			nid[idn] = idl + blockDim.x;
			idn++;
		}
		//within block left neighbour
		if ((int)((idl - 1) % blockDim.x) != (blockDim.x - 1) && label * tex1Dfetch(texTrLabels, id - 1))
		{
			nid[idn] = idl - 1;
			idn++;
		}
		//within block right neighbour
		if ((int)((idl + 1) % blockDim.x) != 0 && label * tex1Dfetch(texTrLabels, id + 1))
		{
			nid[idn] = idl + 1;
			idn++;
		}

		while (ml)
		{
			Ll[idl] = label;
			__syncthreads();
			ml = 0;
			
			
			for (int i = 0; i < idn; i++)
				if (Ll[nid[i]] < label)
				{
					label = Ll[nid[i]];
					ml = 1;
				}
			__syncthreads();
		}

		d_trlabels[id] = label;

		// Size calculation
		unsigned int n = d_trsize[id];
		if(n&&label != id)
		{
			atomicAdd(&d_trsize[label] ,n);
			d_trsize[id] = 0;
		}
	}
}

////////////// ADAPTED FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA //////////////
//// Initialization of id’s, pixels with 0 labels (background) automatically initialized as 0, all others are initialized with 1
__global__ void kernel_initLabels_ccl_improved1(unsigned int *Ld)
{
	unsigned int id = blockIdx.x * blockDim.x + threadIdx.x;
	//unsigned int l = Ld[id] * id;
	unsigned int l = tex1Dfetch(texTrSize, id) * 1;
	Ld[id] = l;
	//Ld[blockIdx.x * blockDim.x + threadIdx.x] = blockIdx.x * blockDim.x + threadIdx.x;
}

////////////// ADAPTED FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA //////////////
//// Initialization with unique id’s, pixels with 0 labels (background) automatically initialized as 0, all others are initialized with global thread id
__global__ void kernel_initLabels_ccl_improved(unsigned int *d_trbinary, long size)
{
	unsigned int gid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;

	for (int i = gid; i < size; i+= offset)
		d_trbinary[i] = d_trbinary[i] * i;
}

////////////// TAKEN FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA (Mesh Kernel A) //////////////
__global__ void kernel_ccl_mesh_B(unsigned char *Dd, unsigned int *Ld, unsigned int *md, int widthgrid, int heightgrid)
{
	int id = (blockIdx.y * blockDim.y + threadIdx.y) * widthgrid + blockIdx.x * blockDim.x + threadIdx.x;
	int idl = threadIdx.y * blockDim.x + threadIdx.x;
	int idn = 0;
	unsigned int label = Ld[id];
	unsigned char state = Dd[id];
	unsigned int nid [4];

	extern __shared__ unsigned int Ll [];
	__shared__ unsigned int ml;
	ml = 1;
	
	//if (id == 0) md[0] = 0;
	//md[0] = 0;

	////// GLOBAL PART ///////
	//upper neighbour
	if ((id - widthgrid) >= 0 && state == Dd[id - widthgrid])
	{
		nid[idn] = Ld[id - widthgrid];
		idn++;
	}
	//lower neighbour
	if ((id + widthgrid) < widthgrid * heightgrid && state == Dd[id + widthgrid])
	{
		nid[idn] = Ld[id + widthgrid];
		idn++;
	}
	//left neighbour
	if (((id - 1) % widthgrid) != (widthgrid - 1) && state == Dd[id - 1])
	{
		nid[idn] = Ld[id - 1];
		idn++;
	}
	//right neighbour
	if (((id + 1) % widthgrid) != 0 && state == Dd[id + 1])
	{
		nid[idn] = Ld[id + 1];
		idn++;
	}

	for (int i = 0; i < idn; i++)
		if (nid[i] < label)
		{
			label = nid[i];
			md[0] = 1;
		}


	///// SHARED PART ///////
	idn = 0;
	//int asd = idl - blockDim.x;
	//within block upper neighbour
	if ((int)(idl - blockDim.x) >= 0 && state == Dd[id - widthgrid])
	{
		nid[idn] = idl - blockDim.x;
		idn++;
	}
	//within block lower neighbour
	if ((int)(idl + blockDim.x) < blockDim.x * blockDim.y && state == Dd[id + widthgrid])
	{
		nid[idn] = idl + blockDim.x;
		idn++;
	}
	//within block left neighbour
	if ((int)((idl - 1) % blockDim.x) != (blockDim.x - 1) && state == Dd[id - 1])
	{
		nid[idn] = idl - 1;
		idn++;
	}
	//within block right neighbour
	if ((int)((idl + 1) % blockDim.x) != 0 && state == Dd[id + 1])
	{
		nid[idn] = idl + 1;
		idn++;
	}

	while (ml)
	{
		Ll[idl] = label;
		__syncthreads();
		ml = 0;
		for (int i = 0; i < idn; i++)
			if (Ll[nid[i]] < label)
			{
				label = Ll[nid[i]];
				ml = 1;
			}
		__syncthreads();
	}
	Ld[id] = label;
}

////////////// TAKEN FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA (Mesh Kernel A) //////////////
__global__ void kernel_ccl_mesh_A(unsigned char *Dd, unsigned int *Ld, unsigned int *md, int widthgrid, int heightgrid)
{
	int id = blockIdx.x * blockDim.x + threadIdx.x;
	int idn = 0;
	unsigned int label = Ld[id];
	unsigned char state = Dd[id];
	unsigned int nid [4];
	
	//upper neighbour
	if ((id - widthgrid) >= 0 && state == Dd[id - widthgrid])
	{
		nid[idn] = Ld[id - widthgrid];
		idn++;
	}
	//lower neighbour
	if ((id + widthgrid) < widthgrid * heightgrid && state == Dd[id + widthgrid])
	{
		nid[idn] = Ld[id + widthgrid];
		idn++;
	}
	//left neighbour
	if (((id - 1) % widthgrid) != (widthgrid - 1) && state == Dd[id - 1])
	{
		nid[idn] = Ld[id - 1];
		idn++;
	}
	//right neighbour
	if (((id + 1) % widthgrid) != 0 && state == Dd[id + 1])
	{
		nid[idn] = Ld[id + 1];
		idn++;
	}

	for (int i = 0; i < idn; i++)
		if (nid[i] < label)
		{
			label = nid[i];
			md[0] = 1;
		}

	Ld[id] = label;
}

////////////// TAKEN FROM PAPER : Parallel Graph Component Labelling with GPUs and CUDA //////////////
//// Initialization with unique id’s
__global__ void kernel_initLabels_ccl(unsigned int *Ld)
{
	Ld[blockIdx.x * blockDim.x + threadIdx.x] = blockIdx.x * blockDim.x + threadIdx.x;
}


/*
////////////// TAKEN FROM PAPER : Connected component labeling on a 2D grid using CUDA //////////////
__global__ void kernel_scanning(unsigned int *_Labels, unsigned int *_isNotDone)
{
	unsigned int id = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int cy = id / SIZEX ;
	unsigned int cx = id - cy * SIZEX;
	unsigned int aPos = (cy + 1) * (SIZEXPAD) + cx + 1;
	unsigned int l = _Labels[aPos];
	if(l)
	{
		unsigned int lw = _Labels[aPos − 1];
		unsigned int minl = ULONG_MAX;
		if (lw) minl = lw;
		unsigned int le = _Labels[aPos + 1];
		if (le&&le < minl) minl = le;
		unsigned int ls = _Labels[aPos − SIZEX − 2];
		if (ls&&ls < minl) minl = ls;
		unsigned int ln = _Labels[aPos + SIZEX + 2];
		if (ln&&ln < minl) minl = ln;
		if (minl < l)
		{
			unsigned int ll = _Labels[l];
			_Labels[l] = min(ll,minl);
			_isNotDone[0] = 1;
		}
	}
}

////////////// TAKEN FROM PAPER : Connected component labeling on a 2D grid using CUDA //////////////
__global__ void kernel_analysis(unsigned int *_Labels, unsigned int *_CSize)
{
	unsigned int id = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int cy = id / SIZEX;
	unsigned int cx = id - cy * SIZEX;
	unsigned int aPos = (cy + 1)*( SIZEXPAD ) + cx + 1;
	unsigned int label = _Labels[aPos];
	if (label)
	{
		unsigned int r = _Labels[label];
		while ( r != label )
		{
			label = _Labels[r];
			r = _Labels[label];
		}
		_Labels[aPos] = label;
		
		// Size calculation
		unsigned int n = _CSize[aPos];
		if(n&&label != aPos)
		{
			atomicAdd(&_CSize[label] ,n);
			_CSize[aPos] = 0;
		}
	}
}


////////////// TAKEN FROM PAPER : Connected component labeling on a 2D grid using CUDA //////////////
//// Initialization with unique id’s
__global__ void kernel_initLabels(unsigned int *_Labels)
{
	unsigned int id = blockIdx.y * gridDim.x * blockDim.x + blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int cy = id / SIZEX;
	unsigned int cx = id - cy * SIZEX;
	unsigned int aPos = (cy+1) * SIZEXPAD + cx + 1;
	unsigned int l = _Labels [aPos];
	l *= aPos;
	_Labels [aPos] = l;
}
*/

/////////////////// HISTOGRAM EQUALIZATION ////////////////////////

__global__ void kernel_histeq(float4 *d_data, uint4 *d_min, unsigned int n_bins, long size)
{
	 int i = threadIdx.x + blockIdx.x * blockDim.x;
	 uint4 min= *d_min;
     int offset = blockDim.x * gridDim.x;
     while (i < size)
     {
		 float4 dummy = d_data[i];
		 float4 dummynew;
		 unsigned int dcdfr = tex1Dfetch(texCdf, dummy.x);
		 unsigned int dcdfg = tex1Dfetch(texCdf, n_bins+dummy.y);
		 unsigned int dcdfb = tex1Dfetch(texCdf, 2*n_bins+dummy.z);
		 dummynew.x = roundf(((dcdfr - min.x)*255.0f)/(size - 1));
		 dummynew.y = roundf(((dcdfg - min.y)*255.0f)/(size - 1));
		 dummynew.z = roundf(((dcdfb - min.z)*255.0f)/(size - 1));
		 dummynew.w = dummy.w;
		 d_data[i] = dummynew;
         i += offset;
     }
}

__global__ void kernel_histeq_tex(float4 *d_data, uint4 *d_min, long size)
{
	 int i = threadIdx.x + blockIdx.x * blockDim.x;
	 uint4 min= *d_min;
     int offset = blockDim.x * gridDim.x;
     while (i < size)
     {
		 float4 dummy = tex1Dfetch(texImg, i);
		 float4 dummynew;
		 unsigned int dcdfr = tex1Dfetch(texCdf, dummy.x);
		 unsigned int dcdfg = tex1Dfetch(texCdf, 256+dummy.y);
		 unsigned int dcdfb = tex1Dfetch(texCdf, 512+dummy.z);
		 dummynew.x = roundf(((dcdfr - min.x)*255.0f)/(size - 1));
		 dummynew.y = roundf(((dcdfg - min.y)*255.0f)/(size - 1));
		 dummynew.z = roundf(((dcdfb - min.z)*255.0f)/(size - 1));
		 dummynew.w = dummy.w;
		 d_data[i] = dummynew;
         i += offset;
     }
}

__global__ void kernel_findmin(unsigned int *d_cdf, uint4 *d_min, unsigned int n_bins)
{
	extern __shared__ unsigned int s_cdf [];
	s_cdf[threadIdx.x] = d_cdf[threadIdx.x];
	s_cdf[n_bins+threadIdx.x] = d_cdf[n_bins+threadIdx.x];
	s_cdf[2*n_bins+threadIdx.x] = d_cdf[2*n_bins+threadIdx.x];

	for (int s = n_bins / 2; s > 0; s >>= 1)
	{
		if (threadIdx.x < s)
		{
			float dr1 = s_cdf[threadIdx.x], dr2 = s_cdf[threadIdx.x+s];
			float dg1 = s_cdf[n_bins+threadIdx.x], dg2 = s_cdf[n_bins+threadIdx.x+s];
			float db1 = s_cdf[2*n_bins+threadIdx.x], db2 = s_cdf[2*n_bins+threadIdx.x+s];
			if (dr1 == 0 || dr2 == 0)
				s_cdf[threadIdx.x] = fmaxf(dr1, dr2);
			else
				s_cdf[threadIdx.x] = fminf(dr1, dr2);
			if (dg1 == 0 || dg2 == 0)
				s_cdf[n_bins+threadIdx.x] = fmaxf(dg1, dg2);
			else
				s_cdf[n_bins+threadIdx.x] = fminf(dg1, dg2);
			if (db1 == 0 || db2 == 0)
				s_cdf[2*n_bins+threadIdx.x] = fmaxf(db1, db2);
			else
				s_cdf[2*n_bins+threadIdx.x] = fminf(db1, db2);
		}
		__syncthreads();
	}

	if (threadIdx.x == 0)
	{
		uint4 dum = make_uint4(s_cdf[threadIdx.x], s_cdf[n_bins+threadIdx.x], s_cdf[2*n_bins+threadIdx.x], 0);
		*d_min = dum;
	}
}

//as described in the prefix scan paper of nvidia (exclusive scan)
//for calculation of cumulative distribution function
//converted to an inclusive scan
__global__ void kernel_prefixsumscan_cdf(unsigned int *d_cdf, unsigned int *d_hist, int n_bins, long size_max) 
{ 	
	extern __shared__ unsigned int temp[];// allocated on invocation 
	int thid = threadIdx.x; 
	
	int offset = 1; 
	
	int ai = thid; 
	int bi = thid + (n_bins/2);
	
	int bankOffsetA = CONFLICT_FREE_OFFSET(ai); 
	int bankOffsetB = CONFLICT_FREE_OFFSET(bi); 

	int cnst = n_bins + CONFLICT_FREE_OFFSET(n_bins - 1);
	
	temp[ai + bankOffsetA] = d_hist[ai]; 
	temp[bi + bankOffsetB] = d_hist[bi]; 
	temp[cnst + ai + bankOffsetA] = d_hist[n_bins + ai]; 
	temp[cnst + bi + bankOffsetB] = d_hist[n_bins + bi];
	temp[2 * cnst + ai + bankOffsetA] = d_hist[2 * n_bins + ai]; 
	temp[2 * cnst + bi + bankOffsetB] = d_hist[2 * n_bins + bi];
	
	for (int d = n_bins>>1; d > 0; d >>= 1) // build sum in place up the tree 
	{ 
		__syncthreads(); 
		if (thid < d) 
		{ 
			int ai = offset*(2*thid+1)-1; 
			int bi = offset*(2*thid+2)-1; 
			ai += CONFLICT_FREE_OFFSET(ai); 
			bi += CONFLICT_FREE_OFFSET(bi);
			temp[bi] += temp[ai]; 
			temp[cnst+bi] += temp[cnst+ai];
			temp[2*cnst+bi] += temp[2*cnst+ai];
		} 
		offset *= 2; 
	} 
	
	if (thid==0) { 
		temp[cnst - 1] = 0;
		temp[2 * cnst - 1] = 0;
		temp[3 * cnst - 1] = 0;
	}
	
	for (int d = 1; d < n_bins; d *= 2) // traverse down tree & build scan 
	{ 
		offset >>= 1; 
		__syncthreads();

		if (thid < d) 
		{ 
			int ai = offset*(2*thid+1)-1; 
			int bi = offset*(2*thid+2)-1; 
			ai += CONFLICT_FREE_OFFSET(ai); 
			bi += CONFLICT_FREE_OFFSET(bi);
			
			unsigned int t = temp[ai]; 
			unsigned int t2 = temp[cnst+ai];
			unsigned int t3 = temp[2*cnst+ai];
			temp[ai] = temp[bi];
			temp[ai+cnst] = temp[bi+cnst];
			temp[ai+2*cnst] = temp[bi+2*cnst];
			temp[bi] += t; 
			temp[bi+cnst] += t2; 
			temp[bi+2*cnst] += t3;
		} 
	} 
	__syncthreads(); 

	if (ai != 0) 
	{
		d_cdf[ai - 1] = temp[ai + bankOffsetA];
		d_cdf[n_bins + ai - 1] = temp[cnst + ai + bankOffsetA];
		d_cdf[2 * n_bins + ai - 1] = temp[2*cnst + ai + bankOffsetA];
	}
	else	
	{
		d_cdf[n_bins - 1] = size_max;
		d_cdf[2 * n_bins - 1] = size_max;
		d_cdf[3 * n_bins - 1] = size_max;
	}
	d_cdf[bi - 1] = temp[bi + bankOffsetB];	 
	d_cdf[n_bins + bi - 1] = temp[cnst + bi + bankOffsetB];	 
	d_cdf[2 * n_bins + bi - 1] = temp[2*cnst + bi + bankOffsetB];

}


/////////////////// PREFIX COMPACT SCAN ////////////////////////

//TODO: Multiblock prefix sum scan implementation (especially for compact)
//TODO: Recode if (thid == blockDim.x - 1) part to if (thid == 0) !!!
__global__ void kernel_prefixsumscan_compact_uint_nontex(unsigned int *g_odata, unsigned int *g_idata, long size_max) 
{ 	
	//global memory id and offset
	unsigned int gid = threadIdx.x;
	unsigned int goffset = blockDim.x * 2;
	//starting sum
	__shared__ unsigned int sum[1];
	sum[0] = 0;

	extern __shared__ unsigned int temp[];// do not forget to change this to n 
	
	while (gid < size_max)
	{
	
		int thid = threadIdx.x; 
	
		int offset = 1; 
	
		int ai = thid; 
		int bi = thid + (blockDim.x);
	
		int bankOffsetA = CONFLICT_FREE_OFFSET(ai); 
		int bankOffsetB = CONFLICT_FREE_OFFSET(bi); 
	
		temp[ai + bankOffsetA] = g_idata[gid]; 
		temp[bi + bankOffsetB] = g_idata[gid + goffset/2]; 
		
		for (int d = blockDim.x; d > 0; d >>= 1) // build sum in place up the tree 
		{	 
			__syncthreads(); 
			if (thid < d) 
			{ 
				int ai = offset*(2*thid+1)-1; 
				int bi = offset*(2*thid+2)-1; 
				ai += CONFLICT_FREE_OFFSET(ai); 
				bi += CONFLICT_FREE_OFFSET(bi);
				temp[bi] += temp[ai]; 
			} 
			offset *= 2; 
		} 
	
		if (thid==0) { 
			temp[2*blockDim.x - 1 + CONFLICT_FREE_OFFSET(2*blockDim.x - 1)] = sum[0];
		}
	
		for (int d = 1; d < 2*blockDim.x; d *= 2) // traverse down tree & build scan 
		{ 
			offset >>= 1; 
			__syncthreads();

			if (thid < d) 
			{ 
				int ai = offset*(2*thid+1)-1; 
				int bi = offset*(2*thid+2)-1; 
				ai += CONFLICT_FREE_OFFSET(ai); 
				bi += CONFLICT_FREE_OFFSET(bi);
			
				unsigned int t = temp[ai]; 
				temp[ai] = temp[bi];
				temp[bi] += t; 
			} 
		} 
		__syncthreads(); 

		g_odata[gid] = temp[ai + bankOffsetA];
		g_odata[gid + goffset/2] = temp[bi + bankOffsetB];

		if (thid == blockDim.x - 1)
		{
			sum[0] = (temp[bi + bankOffsetB] + g_idata[gid + goffset/2]);
		}

		gid += goffset;
	}
}

//TODO: Multiblock prefix sum scan implementation (especially for compact)
//TODO: Recode if (thid == blockDim.x - 1) part to if (thid == 0) !!!
__global__ void kernel_prefixsumscan_compact_uint(unsigned int *d_trcompact, unsigned int *d_trlabels, long size_max) 
{ 	
	//global memory id and offset
	unsigned int gid = threadIdx.x;
	unsigned int goffset = blockDim.x * 2;
	//starting sum
	__shared__ unsigned int sum[1];
	sum[0] = 0;

	extern __shared__ unsigned int temp[];
	
	while (gid < size_max)
	{
	
		int thid = threadIdx.x; 
	
		int offset = 1; 
	
		int ai = thid; 
		int bi = thid + (blockDim.x);
	
		int bankOffsetA = CONFLICT_FREE_OFFSET(ai); 
		int bankOffsetB = CONFLICT_FREE_OFFSET(bi); 
	
		temp[ai + bankOffsetA] = tex1Dfetch(texTrLabels, gid); 
		temp[bi + bankOffsetB] = tex1Dfetch(texTrLabels, gid + goffset/2); 
		
		for (int d = blockDim.x; d > 0; d >>= 1) // build sum in place up the tree 
		{	 
			__syncthreads(); 
			if (thid < d) 
			{ 
				int ai = offset*(2*thid+1)-1; 
				int bi = offset*(2*thid+2)-1; 
				ai += CONFLICT_FREE_OFFSET(ai); 
				bi += CONFLICT_FREE_OFFSET(bi);
				temp[bi] += temp[ai]; 
			} 
			offset *= 2; 
		} 
	
		if (thid==0) { 
			temp[2*blockDim.x - 1 + CONFLICT_FREE_OFFSET(2*blockDim.x - 1)] = sum[0];
		}
	
		for (int d = 1; d < 2*blockDim.x; d *= 2) // traverse down tree & build scan 
		{ 
			offset >>= 1; 
			__syncthreads();

			if (thid < d) 
			{ 
				int ai = offset*(2*thid+1)-1; 
				int bi = offset*(2*thid+2)-1; 
				ai += CONFLICT_FREE_OFFSET(ai); 
				bi += CONFLICT_FREE_OFFSET(bi);
			
				unsigned int t = temp[ai]; 
				temp[ai] = temp[bi];
				temp[bi] += t; 
			} 
		} 
		__syncthreads(); 

		d_trcompact[gid] = temp[ai + bankOffsetA];
		d_trcompact[gid + goffset/2] = temp[bi + bankOffsetB];

		if (thid == blockDim.x - 1)
			sum[0] = (temp[bi + bankOffsetB] + tex1Dfetch(texTrLabels, gid + goffset/2));

		gid += goffset;
	}
}

/*
//TODO: Multiblock prefix sum scan implementation (especially for compact)
__global__ void kernel_prefixsumscan_compact(unsigned int *g_odata, unsigned char *g_idata, int n, long size_max) 
{ 	
	//global memory id and offset
	int gid = threadIdx.x;
	int goffset = n;
	//starting sum
	__shared__ unsigned int sum[1];
	sum[0] = 0;

	__shared__ unsigned int temp[1024];// allocated on invocation 
	
	while (gid < size_max)
	{
	
		int thid = threadIdx.x; 
	
		int offset = 1; 
	
		int ai = thid; 
		int bi = thid + (n/2);
	
		int bankOffsetA = CONFLICT_FREE_OFFSET(ai); 
		int bankOffsetB = CONFLICT_FREE_OFFSET(bi); 
	
		temp[ai + bankOffsetA] = tex1Dfetch(texTrBinary, gid); 
		temp[bi + bankOffsetB] = tex1Dfetch(texTrBinary, gid + goffset/2); 
		
		for (int d = n>>1; d > 0; d >>= 1) // build sum in place up the tree 
		{	 
			__syncthreads(); 
			if (thid < d) 
			{ 
				int ai = offset*(2*thid+1)-1; 
				int bi = offset*(2*thid+2)-1; 
				ai += CONFLICT_FREE_OFFSET(ai); 
				bi += CONFLICT_FREE_OFFSET(bi);
				temp[bi] += temp[ai]; 
			} 
			offset *= 2; 
		} 
	
		if (thid==0) { 
			temp[n - 1 + CONFLICT_FREE_OFFSET(n - 1)] = sum[0];
		}
	
		for (int d = 1; d < n; d *= 2) // traverse down tree & build scan 
		{ 
			offset >>= 1; 
			__syncthreads();

			if (thid < d) 
			{ 
				int ai = offset*(2*thid+1)-1; 
				int bi = offset*(2*thid+2)-1; 
				ai += CONFLICT_FREE_OFFSET(ai); 
				bi += CONFLICT_FREE_OFFSET(bi);
			
				unsigned int t = temp[ai]; 
				temp[ai] = temp[bi];
				temp[bi] += t; 
			} 
		} 
		__syncthreads(); 

		g_odata[gid] = temp[ai + bankOffsetA];
		g_odata[gid + goffset/2] = temp[bi + bankOffsetB];

		
		//__syncthreads();

		if (thid == blockDim.x - 1)
		{
			//sum[0] = (g_odata[gid + goffset/2] + tex1Dfetch(texTrBinary, gid + goffset/2));
			sum[0] = (temp[bi + bankOffsetB] + tex1Dfetch(texTrBinary, gid + goffset/2));
		}

		gid += goffset;
	}
}
*/

///////////////// HISTOGRAM CALCULATION ////////////////////////

__global__ void kernel_hist_tex( unsigned int *d_hist, long size )
{
     __shared__ unsigned int tempr[256];
	 __shared__ unsigned int tempg[256];
	 __shared__ unsigned int tempb[256];
     
	 if (threadIdx.x < 256)
	 {
		tempr[threadIdx.x] = 0;
		tempg[threadIdx.x] = 0;
		tempb[threadIdx.x] = 0;
	 }
     
	 __syncthreads();

     int i = threadIdx.x + blockIdx.x * blockDim.x;
     int offset = blockDim.x * gridDim.x;
     while (i < size)
     {
		 float4 dummy = tex1Dfetch(texImg, i);
		 atomicAdd( &tempr[(unsigned int)dummy.x], 1);
		 atomicAdd( &tempg[(unsigned int)dummy.y], 1);
		 atomicAdd( &tempb[(unsigned int)dummy.z], 1);
         i += offset;
     }
     __syncthreads();

	 if (threadIdx.x < 256)
	 {
		atomicAdd( &(d_hist[threadIdx.x]), tempr[threadIdx.x] );
		atomicAdd( &(d_hist[threadIdx.x+256]), tempg[threadIdx.x] );
		atomicAdd( &(d_hist[threadIdx.x+512]), tempb[threadIdx.x] );
	 }
}

__global__ void kernel_hist( float4 *d_data, unsigned int *d_hist, long size )
{
     __shared__ unsigned int tempr[256];
	 __shared__ unsigned int tempg[256];
	 __shared__ unsigned int tempb[256];
     
	 if (threadIdx.x < 256)
	 {
		tempr[threadIdx.x] = 0;
		tempg[threadIdx.x] = 0;
		tempb[threadIdx.x] = 0;
	 }
     
	 __syncthreads();

     int i = threadIdx.x + blockIdx.x * blockDim.x;
     int offset = blockDim.x * gridDim.x;
     while (i < size)
     {
		 float4 dummy = d_data[i];
		 atomicAdd( &tempr[(unsigned int)dummy.x], 1);
		 atomicAdd( &tempg[(unsigned int)dummy.y], 1);
		 atomicAdd( &tempb[(unsigned int)dummy.z], 1);
         i += offset;
     }
     __syncthreads();

	 if (threadIdx.x < 256)
	 {
		atomicAdd( &(d_hist[threadIdx.x]), tempr[threadIdx.x] );
		atomicAdd( &(d_hist[threadIdx.x+256]), tempg[threadIdx.x] );
		atomicAdd( &(d_hist[threadIdx.x+512]), tempb[threadIdx.x] );
	 }
}

/////////////////////// RGB - HSV CONVERSION ////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! Converts an rgb image to hsv
//! @param d_data  data to process r, g, b, y
//! @param size  size of the image width*height
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_rgb2hsv_2(float4 *d_data, long size)
{
     //map the memory location
	 int offset = 2 * blockDim.x * gridDim.x;
	 unsigned int i = threadIdx.x + blockIdx.x * blockDim.x;
	 unsigned int i2 = threadIdx.x + blockIdx.x * blockDim.x + offset/2;

	 while (i < size && i2 < size)
     {
		float4 dummy = tex1Dfetch( texImg, i );
		float4 dummy2 = tex1Dfetch( texImg, i2 );
    
		//rgb to hsv conversion
		dummy.x /= 255.0f;
		dummy2.x /= 255.0f;
		dummy.y /= 255.0f;
		dummy2.y /= 255.0f;
		dummy.z /= 255.0f;
		dummy2.z /= 255.0f;
		float max = fmaxf(dummy.x,fmaxf(dummy.y,dummy.z));
		float max2 = fmaxf(dummy2.x,fmaxf(dummy2.y,dummy2.z));
		float min = fminf(dummy.x,fminf(dummy.y,dummy.z));
		float min2 = fminf(dummy2.x,fminf(dummy2.y,dummy2.z));
		float delta = max - min;
		float delta2 = max2 - min2;
		//float v = max;
		float s = (max != 0) ? (delta/max) : 0;
		float s2 = (max2 != 0) ? (delta2/max2) : 0;
		__syncthreads();
		float h, h2;
		if (max == 0 || s == 0)
			h = 0;
		else
			if (dummy.x == max)
				h = (dummy.y - dummy.z) / delta;
			else if (dummy.y == max)
				h = 2.0f + (dummy.z - dummy.x) / delta;
			else if (dummy.z == max)
				h = 4.0f + (dummy.x - dummy.y) / delta;

		if (max2 == 0 || s2 == 0)
			h2 = 0;
		else
			if (dummy2.x == max2)
				h2 = (dummy2.y - dummy2.z) / delta2;
			else if (dummy2.y == max2)
				h2 = 2.0f + (dummy2.z - dummy2.x) / delta2;
			else if (dummy2.z == max2)
				h2 = 4.0f + (dummy2.x - dummy2.y) / delta2;

		__syncthreads();
		h *= 60.0f;
		h2 *= 60.0f;
		h = (h < 0) ? h + 360.0f : h;
		h2 = (h2 < 0) ? h2 + 360.0f : h2;
		dummy.x = roundf(h / 2.0f);
		dummy2.x = roundf(h2 / 2.0f);
		dummy.y = roundf(255.0f * s);
		dummy2.y = roundf(255.0f * s2);
		dummy.z = roundf(255.0f * max);
		dummy2.z = roundf(255.0f * max2);
	    d_data[i] = dummy;
		d_data[i2] = dummy2;
		i += offset;
		i2 += offset;
	 }
}

///////////////////////////////////////////////////////////////////////////////
//! Converts an rgb image to hsv
//! @param d_data  data to process r, g, b, y
//! @param size  size of the image width*height
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_rgb2hsv(float4 *d_data, long size)
{
     //map the memory location
	 unsigned int i = threadIdx.x + blockIdx.x * blockDim.x;
     int offset = blockDim.x * gridDim.x;

	 while (i < size)
     {
		float4 dummy = d_data[i];
    
		//rgb to hsv conversion
		dummy.x /= 255.0f;
		dummy.y /= 255.0f;
		dummy.z /= 255.0f;
		float max = fmaxf(dummy.x,fmaxf(dummy.y,dummy.z));
		float min = fminf(dummy.x,fminf(dummy.y,dummy.z));
		float delta = max - min;
		float s = (max != 0) ? (delta/max) : 0;
		__syncthreads();
		float h;
		if (max == 0 || s == 0)
			h = 0;
		else
			if (dummy.x == max)
				h = (dummy.y - dummy.z) / delta;
			else if (dummy.y == max)
				h = 2.0f + (dummy.z - dummy.x) / delta;
			else if (dummy.z == max)
				h = 4.0f + (dummy.x - dummy.y) / delta;

		__syncthreads();
		h *= 60.0f;
		h = (h < 0) ? h + 360.0f : h;
		dummy.x = h / 2.0f;
		dummy.y = 255.0f * s;
		dummy.z = 255.0f * max;
	    d_data[i] = dummy;
		i += offset;
	 }
}

////////////////////////// FEATURE EXTRACTION AND FILTERING ////////////////////////

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! This implementation is 1 patch per thread implementation
//! It also filters the training patches and create a lookup table of size 
//! number of patches, and put a 1 if the patch is a training sample, 0 if not
//! @param d_data		data to process r, g, b, y
//! @param d_hists		resultant histograms of each patch. First npatches
//! values are the the values for first bins, second npatches values are 
//! the values for the second bins, etc.
//! @param d_trbinary	binary array to mark the locations of the training 
//! patches within the array (1 for training patches, 0 for not)
//! @param d_trsize		size array that will be used for the calculation of 
//! biggest connected components later
//! @param d_ntr		total number of training samples gathered
//! @param width		width of the image
//! @param npatches		number of patches in the image
//! @param sizepatch	size of one edge of each image patch in pixels
//! @param threshold_var_y	y variance threshold
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_feature_extraction(float4 *d_data, unsigned int *d_hists, unsigned int *d_trbinary, unsigned int *d_trsize, unsigned int *d_ntr, int width, int npatches, int sizepatch, float threshold_var_y)
{
	//patch id
	unsigned int pid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	//calculate bin intervals 
	int binSat = 256 / N_BINS;
	int binHue = 181 / N_BINS;
	//local histograms
	unsigned int lhist [N_BINS_SQR]; 

	while (pid < npatches)
	{
		unsigned int prow = pid / (width / sizepatch);
		unsigned int pcol = pid % (width / sizepatch);
		unsigned int basepixelid = prow * sizepatch * width + pcol * sizepatch;

		//initialize the local histogram and y sums
		float y = 0.0f;
		float y_sqr = 0.0f;
		for (int i = 0; i < N_BINS_SQR; i++)
			lhist[i] = 0;

		for (int i = 0; i < sizepatch; i++)
			for (int j = 0; j < sizepatch; j++)
			{
				//take the pixel from texture memory
				unsigned int pixelid = basepixelid + i * width + j;
				float4 dummy = tex1Dfetch(texImg, pixelid);
				//fill local histograms and y, y_sqr
				unsigned char noBinHue = (unsigned char)(dummy.x / binHue);
				unsigned char noBinSat = (unsigned char)(dummy.y / binSat);
				lhist[noBinHue * N_BINS + noBinSat]++;
				y += dummy.w;
				y_sqr += dummy.w * dummy.w;
			}
		
		for (int i = 0; i < N_BINS_SQR; i++)
			d_hists[i * npatches + pid] = lhist[i];
		
		//calculate height variance
		float var = (y_sqr - (y*y)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		
		if (var != 0 && var < threshold_var_y)
		{
			d_trbinary[pid] = 1;
			d_trsize[pid] = 1;
			atomicAdd(d_ntr, 1); 
		}

		pid += offset;
	}
}

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! This implementation is 1 patch per thread implementation
//! It also filters the training patches and create a lookup table of size number of patches, and put a 1 if the patch is a training samples, 0 if not
//! @param d_data		data to process r, g, b, y
//! @param d_hists		resultant histograms of each patch in the form of consecutive int4's
//! @param d_trbinary	binary array to mark the locations of the training patches within the array (1 for training patches, 0 for not)
//! @param d_ntr		number of training samples gathered
//! @param width		width of the image
//! @param npatches		number of patches in the image
//! @param nbins		number of bins in each channel of the histogram
//! @param sizepatch	size of one edge of each image patch in pixels
//! @param threshold_var_y	//y variance threshold
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_downsample_4_trlocs_uint(float4 *d_data, unsigned int *d_hists, unsigned int *d_trbinary, unsigned int *d_trsize, unsigned int *d_ntr, int width, int npatches, int sizepatch, float threshold_var_y)
{
	//patch id
	unsigned int pid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	//calculate the bins
	int binSat = 256 / N_BINS;
	int binHue = 181 / N_BINS;
	unsigned int lhist [N_BINS_SQR]; 

	while (pid < npatches)
	{
		unsigned int prow = pid / (width / sizepatch);
		unsigned int pcol = pid % (width / sizepatch);
		unsigned int basepixelid = prow * sizepatch * width + pcol * sizepatch;

		//initialize the local histogram and y sums
		float y = 0.0f;
		float y_sqr = 0.0f;
		for (int i = 0; i < N_BINS_SQR; i++)
			lhist[i] = 0;

		for (int i = 0; i < sizepatch; i++)
			for (int j = 0; j < sizepatch; j++)
			{
				//take the pixel from texture memory
				unsigned int pixelid = basepixelid + i * width + j;
				float4 dummy = tex1Dfetch(texImg, pixelid);
				//fill local histograms and y, y_sqr
				unsigned char noBinHue = (unsigned char)(dummy.x / binHue);
				unsigned char noBinSat = (unsigned char)(dummy.y / binSat);
				lhist[noBinHue * N_BINS + noBinSat]++;
				y += dummy.w;
				y_sqr += dummy.w * dummy.w;
			}

		//for (int i = 0; i < N_BINS_SQR / 4; i++)
		//	d_hists[pid * N_BINS_SQR / 4 + i] = make_uint4(lhist[i * 4], lhist[i * 4 + 1], lhist[i * 4 + 2], lhist[i * 4 + 3]);
		for (int i = 0; i < N_BINS_SQR; i++)
			d_hists[i * npatches + pid] = lhist[i];
		//d_vars[pid] = (y_sqr - (y*y)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		float var = (y_sqr - (y*y)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		if (var != 0 && var < threshold_var_y)
		{
			d_trbinary[pid] = 1;
			d_trsize[pid] = 1;
			atomicAdd(d_ntr, 1); 
		}

		pid += offset;
	}
}

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! This implementation is 1 patch per thread implementation
//! It also filters the training patches and create a lookup table of size number of patches, and put a 1 if the patch is a training samples, 0 if not
//! @param d_data		data to process r, g, b, y
//! @param d_hists		
//! @param d_trbinary	binary array to mark the locations of the training patches within the array (1 for training patches, 0 for not)
//! @param d_ntr		number of training samples gathered
//! @param width		width of the image
//! @param npatches		number of patches in the image
//! @param nbins		number of bins in each channel of the histogram
//! @param sizepatch	size of one edge of each image patch in pixels
//! @param threshold_var_y	//y variance threshold
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_downsample_4_trlocs(float4 *d_data, unsigned int *d_hists, unsigned char *d_trbinary, unsigned int *d_ntr, int width, int npatches, int sizepatch, float threshold_var_y)
{
	//patch id
	unsigned int pid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	//calculate the bins
	int binSat = 256 / N_BINS;
	int binHue = 181 / N_BINS;
	unsigned char lhist [N_BINS_SQR]; 

	while (pid < npatches)
	{
		unsigned int prow = pid / (width / sizepatch);
		unsigned int pcol = pid % (width / sizepatch);
		unsigned int basepixelid = prow * sizepatch * width + pcol * sizepatch;

		//initialize the local histogram and y sums
		float y = 0.0f;
		float y_sqr = 0.0f;
		for (int i = 0; i < N_BINS_SQR; i++)
			lhist[i] = 0;

		for (int i = 0; i < sizepatch; i++)
			for (int j = 0; j < sizepatch; j++)
			{
				//take the pixel from texture memory
				unsigned int pixelid = basepixelid + i * width + j;
				float4 dummy = tex1Dfetch(texImg, pixelid);
				//fill local histograms and y, y_sqr
				unsigned char noBinHue = (unsigned char)(dummy.x / binHue);
				unsigned char noBinSat = (unsigned char)(dummy.y / binSat);
				lhist[noBinHue * N_BINS + noBinSat]++;
				y += dummy.w;
				y_sqr += dummy.w * dummy.w;
			}

		for (int i = 0; i < N_BINS_SQR; i++)
			d_hists[i * npatches + pid] = lhist[i];
		//d_vars[pid] = (y_sqr - (y*y)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		float var = (y_sqr - (y*y)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		if (var != 0 && var < threshold_var_y)
		{
			d_trbinary[pid] = 255;
			atomicAdd(d_ntr, 1); 
		}

		pid += offset;
	}
}

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! This implementation is 1 patch per thread implementation
//! @param d_data		data to process r, g, b, y
//! @param d_hists		resultant histograms of each patch in the form of consecutive int4's
//! @param d_vars		resultant y variances of each patch
//! @param width		width of the image
//! @param npatches		number of patches in the image
//! @param nbins		number of bins in each channel of the histogram
//! @param sizepatch	size of one edge of each image patch in pixels
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_downsample_4(float4 *d_data, uint4 *d_hists, float *d_vars, int width, int npatches, int sizepatch)
{
	//patch id
	unsigned int pid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned int offset = gridDim.x * blockDim.x;
	//calculate the bins
	int binSat = 256 / N_BINS;
	int binHue = 181 / N_BINS;
	unsigned char lhist [N_BINS_SQR]; 

	while (pid < npatches)
	{
		unsigned int prow = pid / (width / sizepatch);
		unsigned int pcol = pid % (width / sizepatch);
		unsigned int basepixelid = prow * sizepatch * width + pcol * sizepatch;

		//initialize the local histogram and y sums
		float y = 0.0f;
		float y_sqr = 0.0f;
		for (int i = 0; i < N_BINS_SQR; i++)
			lhist[i] = 0;

		for (int i = 0; i < sizepatch; i++)
			for (int j = 0; j < sizepatch; j++)
			{
				//take the pixel from texture memory
				unsigned int pixelid = basepixelid + i * width + j;
				float4 dummy = tex1Dfetch(texImg, pixelid);
				//fill local histograms and y, y_sqr
				unsigned char noBinHue = (unsigned char)(dummy.x / binHue);
				unsigned char noBinSat = (unsigned char)(dummy.y / binSat);
				lhist[noBinHue * N_BINS + noBinSat]++;
				y += dummy.w;
				y_sqr += dummy.w * dummy.w;
			}

		for (int i = 0; i < N_BINS_SQR / 4; i++)
			d_hists[pid * N_BINS_SQR / 4 + i] = make_uint4(lhist[i * 4], lhist[i * 4 + 1], lhist[i * 4 + 2], lhist[i * 4 + 3]);
		d_vars[pid] = (y_sqr - (y*y)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		pid += offset;
	}
}

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! This implementation is n patches per block implementation
//! @param d_data		data to process r, g, b, y
//! @param d_hists		resultant histograms of each patch in the form of consecutive int4's
//! @param d_vars		resultant y variances of each patch
//! @param width		width of the image
//! @param nblocks		number of patch blocks in the image
//! @param nbins		number of bins in each channel of the histogram
//! @param sizepatch	size of one edge of each image patch in pixels
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_downsample_3(float4 *d_data, uint4 *d_hists, float *d_vars, int width, int nblocks, int sizepatch)
{
	int offset = gridDim.x;
    //map the patch location
	//within block thread id
	unsigned int tid = threadIdx.y * blockDim.x + threadIdx.x;
	//within block patch id
	unsigned int bpatchid = (threadIdx.x/sizepatch)%(blockDim.x/sizepatch);
	//within patch thread id
	unsigned int ptid = (tid/blockDim.x) * sizepatch + (tid % sizepatch);
	unsigned int bid = blockIdx.x;
	unsigned int smemid = bpatchid * sizepatch * sizepatch + ptid;
	//calculate the bins
	int binSat = 256 / N_BINS;
	int binHue = 181 / N_BINS;
	//shared memory allocations
	extern __shared__ unsigned char s_hist[];
	__shared__ float s_y[SIZE_HIST_IMG];
	__shared__ float s_ysqr[SIZE_HIST_IMG];

	while (bid < nblocks)
	{
		//block col id
		unsigned int blockrow = bid / (width / blockDim.x);
		unsigned int blockcol = bid % (width / blockDim.x);
		//global pixel id
		unsigned int pixid = blockrow * blockDim.y * width + blockcol * blockDim.x + threadIdx.y * width + threadIdx.x;
		float4 dummy = tex1Dfetch(texImg, pixid);
		
		//histogram calculation
		//initialize local histograms
		for (int i = 0; i < N_BINS_SQR; i++)
			s_hist[smemid * N_BINS_SQR + i] = 0;
		//fill local histograms
		int noBinHue = (int)(dummy.x / binHue);
		int noBinSat = (int)(dummy.y / binSat);
		int asd = (smemid * N_BINS + noBinHue) * N_BINS + noBinSat;
		s_hist[asd]++;
		//fill y coord vals
		s_y[smemid] = dummy.w;
		s_ysqr[smemid] = dummy.w * dummy.w;
	
		__syncthreads();

		//reduce to the patch histogram
		for (int s = sizepatch * sizepatch / 2; s > 0; s >>= 1)
		{
			if (ptid < s)
			{
				for (int j = 0; j < N_BINS_SQR; j++)
					s_hist[smemid * N_BINS_SQR + j] += s_hist[(smemid + s) * N_BINS_SQR + j];
				s_y[smemid] += s_y[smemid + s];
				s_ysqr[smemid] += s_ysqr[smemid + s];
			}
			__syncthreads();		
		}
		 
		if (ptid == 0)
		{
			const int stride = N_BINS_SQR / 4;
			for (int i = 0; i < stride; i++)
			{
				d_hists[(bid * (blockDim.x / sizepatch) + bpatchid) * stride + i] = make_uint4(s_hist[smemid*N_BINS_SQR+i*4],s_hist[smemid*N_BINS_SQR+i*4+1],s_hist[smemid*N_BINS_SQR+i*4+2],s_hist[smemid*N_BINS_SQR+i*4+3]);
			}
			float sum = s_y[smemid];
			d_vars[bid * (blockDim.x / sizepatch) + bpatchid] = (s_ysqr[smemid] - (sum*sum)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		}
		//__syncthreads();
		bid += offset;
	 }
}

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! This implementation is n patches per block implementation
//! @param d_data		data to process r, g, b, y
//! @param d_hists		resultant histograms of each patch in the form of consecutive int4's
//! @param d_vars		resultant y variances of each patch
//! @param width		width of the image
//! @param nblocks		number of patch blocks in the image
//! @param nbins		number of bins in each channel of the histogram
//! @param sizepatch	size of one edge of each image patch in pixels
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_downsample_2(float4 *d_data, uint4 *d_hists, float *d_vars, int width, int nblocks, int nbins, int sizepatch)
{
    //map the patch location
	//within block thread id
	unsigned int tid = threadIdx.y * blockDim.x + threadIdx.x;
	//within block patch id
	unsigned int bpatchid = (threadIdx.x/sizepatch)%(blockDim.x/sizepatch);
	//within patch thread id
	unsigned int ptid = (tid/blockDim.x) * sizepatch + (tid % sizepatch);
	unsigned int bid = blockIdx.x;
	unsigned int smemid = bpatchid * sizepatch * sizepatch + ptid;
    int offset = gridDim.x;
	//calculate the bins
	int binSat = 256 / nbins;
	int binHue = 181 / nbins;
	//shared memory allocations
	extern __shared__ unsigned char s_hist[];
	__shared__ float s_y[SIZE_HIST_IMG];
	__shared__ float s_ysqr[SIZE_HIST_IMG];

	while (bid < nblocks)
	{
		//block col id
		unsigned int blockrow = bid / (width / blockDim.x);
		//global pixel id
		unsigned int pixid = blockrow * blockDim.y * width + threadIdx.y * width + blockrow * blockDim.x + threadIdx.x;
		float4 dummy = tex1Dfetch(texImg, pixid);
		
		//histogram calculation
		//initialize local histograms
		for (int i = 0; i < nbins * nbins; i++)
			s_hist[tid * nbins * nbins + i] = 0;
		//fill local histograms
		int noBinHue = (int)(dummy.x / binHue);
		int noBinSat = (int)(dummy.y / binSat);
		int asd = (smemid * nbins + noBinHue) * nbins + noBinSat;
		s_hist[asd]++;
		//fill y coord vals
		s_y[smemid] = dummy.w;
		s_ysqr[smemid] = dummy.w * dummy.w;
	
		__syncthreads();

		//reduce to the patch histogram
		for (int s = blockDim.y * sizepatch / 2; s > 0; s >>= 1)
		{
			if (tid < s * blockDim.x / sizepatch)
			{
				for (int j = 0; j < nbins * nbins; j++)
					s_hist[(tid + (tid / s) * s) * nbins * nbins + j] += s_hist[(tid + ((tid / s) + 1) * s) * nbins * nbins + j];
				s_y[(tid + (tid / s) * s)] += s_y[(tid + ((tid / s) + 1) * s)];
				s_ysqr[(tid + (tid / s) * s)] += s_ysqr[(tid + ((tid / s) + 1) * s)];
			}
			__syncthreads();		
		}
		 
		if (tid < blockDim.x / sizepatch)
		{
			int stride = nbins * nbins / 4;
			for (int i = 0; i < stride; i++)
			{
				d_hists[(bid * (blockDim.x / sizepatch) + bpatchid) * stride + i] = make_uint4(s_hist[tid+i*4],s_hist[tid+i*4+1],s_hist[tid+i*4+2],s_hist[tid+i*4+3]);
			}
			float sum = s_y[tid];
			d_vars[bid * (blockDim.x / sizepatch) + bpatchid] = (s_ysqr[tid] - (sum*sum)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		}
		__syncthreads();
		bid += offset;
	 }
}

///////////////////////////////////////////////////////////////////////////////
//! Calculates the h-s joint histograms and y variances of each image patch
//! @param d_data  memory to process (in and out) r, g, b, y
//! @param size  total size of the image
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_downsample(float4 *d_data, uint4 *d_hists, float *d_vars, int width, int widthp, int height, int heightp, int npatches, int nbins, int sizepatch)
{
     //map the memory location
	 unsigned int patchid = blockIdx.x;
     int offset = gridDim.x;
	 //shared memory allocations
	 extern __shared__ unsigned char s_hist[];
	 //__shared__ unsigned char s_hist[1024];
	 __shared__ float s_y[SIZE_HIST_IMG];
	 __shared__ float s_ysqr[SIZE_HIST_IMG];

	 while (patchid < npatches)
	 {
		//histogram calculation
		//within block thread id
		unsigned int tid = threadIdx.y * blockDim.x + threadIdx.x;
		unsigned int patchrow = patchid / widthp;
		unsigned int patchcol = patchid % widthp;
		//pixel id in memory
		unsigned int pixid = patchrow * blockDim.y * width + threadIdx.y * width + patchcol * blockDim.x + threadIdx.x; 
		float4 dummy = tex1Dfetch(texImg, pixid);
		//calculate the bins
		int binSat = 256 / nbins;
		int binHue = 181 / nbins;
		//initialize local histograms
		for (int i = 0; i < nbins * nbins; i++)
			s_hist[tid * nbins * nbins + i] = 0;
		//fill local histograms
		int noBinHue = (int)(dummy.x / binHue);
		int noBinSat = (int)(dummy.y / binSat);
		s_hist[(tid * nbins + noBinHue) * nbins + noBinSat]++;
		//fill y coord vals
		s_y[tid] = dummy.w;
		s_ysqr[tid] = dummy.w * dummy.w;
	
		__syncthreads();

		//reduce to the patch histogram
		for (int i = blockDim.x * blockDim.y / 2; i > 0; i >>= 1)
		{
			if (tid < i)
			{
				for (int j = 0; j < nbins * nbins; j++)
					s_hist[tid * nbins * nbins + j] += s_hist[(tid + i) * nbins * nbins + j];
				s_y[tid] += s_y[tid+i];
				s_ysqr[tid] += s_ysqr[tid+i];
			}
			__syncthreads();		
		}
		 
		if (tid == 0)
		{
			int stride = nbins * nbins / 4;
			//int ind = blockIdx.y * gridDim.x + blockIdx.x;
			for (int i = 0; i < stride; i++)
			{
				d_hists[patchid*stride+i] = make_uint4(s_hist[tid+i*4],s_hist[tid+i*4+1],s_hist[tid+i*4+2],s_hist[tid+i*4+3]);
				//d_hists[ind+0] = make_int4(hist[tid+x*4],hist[tid+x*4+1],hist[tid+x*4+2],hist[tid+x*4+3]);
			}
			float sum = s_y[tid];
			d_vars[patchid] = (s_ysqr[tid] - (sum*sum)/(sizepatch*sizepatch))/(sizepatch*sizepatch - 1);
		}
		patchid += offset;
	 }
}

//////////////// RGB - HSV CONVERSION + FEATURE EXTRACTION //////////////////

///////////////////////////////////////////////////////////////////////////////
//! Naive implementation
//! @param g_data  memory to process (in and out) r, g, b, y
///////////////////////////////////////////////////////////////////////////////
__global__ void kernel_naive_2(float4 *d_data, uint4 *d_hists, float *d_vars, const int size_patch, const int n_bins)
{
	//map the memory location
    const unsigned int id = ((blockIdx.y * blockDim.y + threadIdx.y) * gridDim.x + blockIdx.x) * blockDim.x + threadIdx.x;
    //float4 dummy = d_data[id];
	float4 dummy = tex1Dfetch( texImg, id );
    
    //rgb to hsv conversion
    dummy.x /= 255.0f;
    dummy.y /= 255.0f;
    dummy.z /= 255.0f;
    float max = fmaxf(dummy.x,fmaxf(dummy.y,dummy.z));
    float min = fminf(dummy.x,fminf(dummy.y,dummy.z));
    float delta = max - min;
    //float v = max;
    float s = (max != 0) ? (delta/max) : 0;
	__syncthreads();
    float h;
    if (max == 0 || s == 0)
		h = 0;
	else
		if (dummy.x == max)
			h = (dummy.y - dummy.z) / delta;
		else if (dummy.y == max)
			h = 2.0f + (dummy.z - dummy.x) / delta;
		else if (dummy.z == max)
			h = 4.0f + (dummy.x - dummy.y) / delta;

	__syncthreads();
	h *= 60.0f;
	h = (h < 0) ? h + 360.0f : h;
	dummy.x = h / 2.0f;
	dummy.y = 255.0f * s;
	dummy.z = 255.0f * max;
	
	//histogram calculation
	//within block thread id
	unsigned int tid = threadIdx.y * blockDim.x + threadIdx.x;
	//calculate the bins
	int binSat = 256 / n_bins;
	int binHue = 181 / n_bins;
	//shared memory allocations
	extern __shared__ unsigned char s_hist[];
	//__shared__ unsigned char s_hist[1024];
	__shared__ float s_y[SIZE_HIST_IMG];
	__shared__ float s_ysqr[SIZE_HIST_IMG];
	//initialize local histograms
	for (int i = 0; i < n_bins * n_bins; i++)
		s_hist[tid * n_bins * n_bins + i] = 0;
	//fill local histograms
	int noBinHue = (int)(dummy.x / binHue);
	int noBinSat = (int)(dummy.y / binSat);
	s_hist[(tid * n_bins + noBinHue) * n_bins + noBinSat]++;
	//fill y coord vals
	s_y[tid] = dummy.w;
	s_ysqr[tid] = dummy.w * dummy.w;
	
	__syncthreads();

	//reduce to the patch histogram
	for (int i = blockDim.x * blockDim.y / 2; i > 0; i >>= 1)
	{
		if (tid < i)
		{
			for (int j = 0; j < n_bins * n_bins; j++)
				s_hist[tid * n_bins * n_bins + j] += s_hist[(tid + i) * n_bins * n_bins + j];
			s_y[tid] += s_y[tid+i];
			s_ysqr[tid] += s_ysqr[tid+i];
		}
		__syncthreads();		
	}
		 
	if (tid == 0)
	{
		int stride = n_bins * n_bins / 4;
		int ind = blockIdx.y * gridDim.x + blockIdx.x;
		for (int i = 0; i < stride; i++)
		{
			d_hists[ind*stride+i] = make_uint4(s_hist[tid+i*4],s_hist[tid+i*4+1],s_hist[tid+i*4+2],s_hist[tid+i*4+3]);
			//d_hists[ind+0] = make_int4(hist[tid+x*4],hist[tid+x*4+1],hist[tid+x*4+2],hist[tid+x*4+3]);
		}
		float sum = s_y[tid];
		d_vars[ind] = (s_ysqr[tid] - (sum*sum)/(size_patch*size_patch))/(size_patch*size_patch - 1);
	}
    d_data[id] = dummy;
}

void insertRandIndices(unsigned int *iRand, unsigned int *ntr)
{
	srand(time(NULL));
	//srand(0);
	for (int i = 0; i < N_CLUSTERS_INITIAL; i++)
	{
		if (i == 0)
		{
			int r = rand() % ntr[0];
			iRand[i] = r;
		}
		else
		{
			bool isRandNotFound = true;
			while (isRandNotFound)
			{
				isRandNotFound = false;
				int r = rand() % ntr[0];
				for (int j = 0; j < i; j++)
				{
					if (r == iRand[j])
					{
						isRandNotFound = true;
						break;
					}
				}
				iRand[i] = r;
			}
		}
	}
}

extern "C" void
initialize(const int argc, const char **argv)
{
	// use command-line specified CUDA device, otherwise use device with highest Gflops/s
    findCudaDevice(argc, (const char **)argv);

	//number of pixels
	unsigned int n_pix = WIDTH * HEIGHT;
	//number of 4-element histogram pieces
	unsigned int n_hists = n_pix / SIZE_PATCH / SIZE_PATCH * N_BINS_SQR;
	//number of patches
	unsigned int n_patches = n_pix / SIZE_PATCH / SIZE_PATCH;

	// calculate the required size to allocate from device memory
    mem_size_data = sizeof(float4) * n_pix;
    mem_size_hists = sizeof(unsigned int) * n_hists;
	mem_size_vars = sizeof(float) * n_patches;
	mem_size_ghist = sizeof(unsigned int) * 256 * 3;
	mem_size_cdf = sizeof(unsigned int) * 256 * 3;
	mem_size_trlabels = sizeof(unsigned int) * n_patches;
	mem_size_trsize = sizeof(unsigned int) * n_patches;
	mem_size_trcompact = sizeof(unsigned int) * n_patches;
	mem_size_classified = sizeof(unsigned int) * n_patches;
	mem_size_means = sizeof(float) * N_BINS_SQR * N_CLUSTERS_MAX;
	mem_size_invCovs = sizeof(float) * N_BINS_SQR * N_CLUSTERS_MAX;
	mem_size_weights = sizeof(float) * N_CLUSTERS_MAX;
	mem_size_probs = sizeof(float) * N_CLUSTERS_MAX * n_patches;
	mem_size_n = sizeof(float) * N_CLUSTERS_MAX;
	mem_size_means_new = sizeof(float) * N_BINS_SQR * N_CLUSTERS_NEW;
	mem_size_invCovs_new = sizeof(float) * N_BINS_SQR * N_CLUSTERS_NEW;
	mem_size_weights_new = sizeof(float) * N_CLUSTERS_NEW;
	mem_size_probs_new = sizeof(float) * N_CLUSTERS_NEW * n_patches;
	mem_size_n_new = sizeof(float) * N_CLUSTERS_NEW;
	mem_size_irand = sizeof(unsigned int) * N_CLUSTERS_MAX;
	
	// Memory allocation on device
    checkCudaErrors(cudaMalloc((void **) &d_data, mem_size_data));
    checkCudaErrors(cudaMalloc((void **) &d_hists, mem_size_hists));
	checkCudaErrors(cudaMalloc((void **) &d_vars, mem_size_vars));
	checkCudaErrors(cudaMalloc((void **) &d_ghist, mem_size_ghist));
	checkCudaErrors(cudaMalloc((void **) &d_cdf, mem_size_cdf));
	checkCudaErrors(cudaMalloc((void **) &d_trbuffer, mem_size_hists));
	checkCudaErrors(cudaMalloc((void **) &d_trremaining, mem_size_hists));
	checkCudaErrors(cudaMalloc((void **) &d_trinds, mem_size_trlabels));
	checkCudaErrors(cudaMalloc((void **) &d_min, sizeof(uint4)));
	checkCudaErrors(cudaMalloc((void **) &d_trcompact, mem_size_trcompact));
	checkCudaErrors(cudaMalloc((void **) &d_trlabels, mem_size_trlabels));
	checkCudaErrors(cudaMalloc((void **) &d_trsize, mem_size_trsize));
	checkCudaErrors(cudaMalloc((void **) &d_classified, mem_size_classified));
	checkCudaErrors(cudaMalloc((void **) &d_roadlabel, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_max, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_max_old, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_max_gid, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_ntr, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_means, mem_size_means));
	checkCudaErrors(cudaMalloc((void **) &d_invCovs, mem_size_invCovs));
	checkCudaErrors(cudaMalloc((void **) &d_weights, mem_size_weights));
	checkCudaErrors(cudaMalloc((void **) &d_probs, mem_size_probs));
	checkCudaErrors(cudaMalloc((void **) &d_n, mem_size_n));
	checkCudaErrors(cudaMalloc((void **) &d_means_new, mem_size_means_new));
	checkCudaErrors(cudaMalloc((void **) &d_invCovs_new, mem_size_invCovs_new));
	checkCudaErrors(cudaMalloc((void **) &d_weights_new, mem_size_weights_new));
	checkCudaErrors(cudaMalloc((void **) &d_probs_new, mem_size_probs_new));
	checkCudaErrors(cudaMalloc((void **) &d_n_new, mem_size_n_new));
	checkCudaErrors(cudaMalloc((void **) &d_ind_clusters_min_weights, sizeof(unsigned int) * N_CLUSTERS_MAX));
	checkCudaErrors(cudaMalloc((void **) &d_irand, mem_size_irand));
	checkCudaErrors(cudaMalloc((void **) &m_d_IsNotDone, sizeof(unsigned int)));

	//bind data to the texture
	cudaBindTexture( 0, texImg, d_data, mem_size_data );
	cudaBindTexture( 0, texCdf, d_cdf, mem_size_cdf );
	cudaBindTexture( 0, texTrLabels, d_trlabels, mem_size_trlabels );
	cudaBindTexture( 0, texTrSize, d_trsize, mem_size_trsize );
	cudaBindTexture( 0, texTrCompact, d_trcompact, mem_size_trcompact );
	cudaBindTexture( 0, texClassified, d_classified, mem_size_classified );
	
}

extern "C" void
cleanup()
{
	//unbind the texture
	cudaUnbindTexture(texImg);
	cudaUnbindTexture(texCdf);
	cudaUnbindTexture(texTrLabels);
	cudaUnbindTexture(texTrSize);
	cudaUnbindTexture(texTrCompact);
	cudaUnbindTexture(texClassified);

	// cleanup memory
    checkCudaErrors(cudaFree(d_data));
    checkCudaErrors(cudaFree(d_hists));
	checkCudaErrors(cudaFree(d_vars));
	checkCudaErrors(cudaFree(d_ghist));
	checkCudaErrors(cudaFree(d_cdf));
	checkCudaErrors(cudaFree(d_trbuffer));
	checkCudaErrors(cudaFree(d_trremaining));
	checkCudaErrors(cudaFree(d_trinds));
	checkCudaErrors(cudaFree(d_min));;
	checkCudaErrors(cudaFree(d_trlabels));
	checkCudaErrors(cudaFree(d_trsize));
	checkCudaErrors(cudaFree(d_max));
	checkCudaErrors(cudaFree(d_max_old));
	checkCudaErrors(cudaFree(d_max_gid));
	checkCudaErrors(cudaFree(d_trcompact));
	checkCudaErrors(cudaFree(d_classified));
	checkCudaErrors(cudaFree(d_roadlabel));
	checkCudaErrors(cudaFree(d_ntr));
	checkCudaErrors(cudaFree(d_means));
	checkCudaErrors(cudaFree(d_invCovs));
	checkCudaErrors(cudaFree(d_weights));
	checkCudaErrors(cudaFree(d_probs));
	checkCudaErrors(cudaFree(d_n));
	checkCudaErrors(cudaFree(d_means_new));
	checkCudaErrors(cudaFree(d_invCovs_new));
	checkCudaErrors(cudaFree(d_weights_new));
	checkCudaErrors(cudaFree(d_probs_new));
	checkCudaErrors(cudaFree(d_n_new));
	checkCudaErrors(cudaFree(d_ind_clusters_min_weights));
	checkCudaErrors(cudaFree(d_irand));

}

////////////////////////////////////////////////////////////////////////////////
//! Entry point for Cuda functionality on host side
//! @param argc  command line argument count
//! @param argv  command line arguments
//! @param data  data to process on the device
//! @param len   len of \a data
////////////////////////////////////////////////////////////////////////////////
extern "C" float
detectRoad(const int argc, const char **argv, float4 *data, unsigned int *n_models, bool isFirstFrame, unsigned int *classified)
{
	//General variables
	//number of pixels
	unsigned int n_pix = WIDTH * HEIGHT;
	//number of patches
	unsigned int n_patches = n_pix / SIZE_PATCH / SIZE_PATCH;
	//number of 4-element histogram pieces
	unsigned int n_hists = n_patches * N_BINS_SQR;
	 

	//General Variables
	const unsigned int n_threads = 256;
	const unsigned int n_blocks = 12;
	//CCL Variables
	//number of threads to be used in connected component labeling
	const unsigned int n_threads_x_ccl = 16;
	const unsigned int n_threads_y_ccl = 32;
	//EM Variables
	//number of blocks to be used in em algorithm
	unsigned int n_blocks_em = 2;
	//em termination criterion for log-likelihood change
	float term_criterion_em = 2.0f;
	//number of training samples
	unsigned int *ntr = new unsigned int[1];
	*ntr = 0;
	//random indices
	unsigned int *iRand = new unsigned int[N_CLUSTERS_MAX];
	//likelihood array
	float *likelihood = new float[n_blocks_em];
	//temporary model weight array
	float *weights = new float[N_CLUSTERS_MAX];
	float *weights_new = new float[N_CLUSTERS_NEW];

	////// CCL STUFF //////
	unsigned int m_IsNotDone = 1;
	unsigned int ite = 0;

	//Get device properties and set the block size accordingly
	cudaDeviceProp prop;
	checkCudaErrors( cudaGetDeviceProperties( &prop, 0 ) );
	//int blocks = prop.multiProcessorCount;
	
	//gpu timer
	StopWatchInterface *timer = NULL;
    float elapsedTimeInMs = 0.0f;
    cudaEvent_t start, stop;
    sdkCreateTimer(&timer);
    checkCudaErrors(cudaEventCreate(&start));
    checkCudaErrors(cudaEventCreate(&stop));

	checkCudaErrors(cudaMemset( d_ghist, 0, mem_size_ghist));
	checkCudaErrors(cudaMemset( d_cdf, 0, mem_size_cdf));
	checkCudaErrors(cudaMemset( d_trlabels, 0, mem_size_trlabels));
	checkCudaErrors(cudaMemset( d_trinds, 0, mem_size_trlabels));
	checkCudaErrors(cudaMemset( d_trsize, 0, mem_size_trsize));
	checkCudaErrors(cudaMemset( d_ntr, 0, sizeof(unsigned int)));
	checkCudaErrors(cudaMemset( d_max, 0, sizeof(unsigned int)));

    //start the timer
    sdkStartTimer(&timer);
    checkCudaErrors(cudaEventRecord(start, 0));
    
    // copy image data to device
    checkCudaErrors(cudaMemcpy(d_data, data, mem_size_data, cudaMemcpyHostToDevice));
    
	//////////////// ALGORITHM BEGINS ////////////////

	///////////////// PRE-PROCESSING //////////////////
	kernel_hist_tex<<< n_blocks, n_threads >>>(d_ghist, n_pix);
	kernel_prefixsumscan_cdf<<< 1, 256/2, sizeof(unsigned int)*(3 * 256 + 1)>>>(d_cdf, d_ghist, 256, n_pix);
	kernel_findmin<<<1, 256, sizeof(unsigned int)*3 * 256>>>(d_cdf, d_min, 256);
	kernel_histeq<<< n_blocks, n_threads >>>(d_data, d_min, 256, n_pix);
	kernel_rgb2hsv<<< n_blocks, n_threads >>>(d_data, n_pix);

	//////////////// FEATURE EXTRACTION ///////////////
	kernel_feature_extraction<<< n_blocks, n_threads >>>(d_data, d_hists, d_trlabels, d_trsize, d_ntr, WIDTH, n_patches, SIZE_PATCH, THRESHOLD_VAR_Y);

	//debug
	//unsigned int *trlabels = new unsigned int[n_patches];
	//unsigned int *trsize = new unsigned int[n_patches];
	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//float maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trlabels.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	//debug
	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//float maxSz = 0, minSz = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	//cout << "Max in trsize: " << maxSz << endl;
	//cout << "Min in trsize: " << minSz << endl;
	//writePPMGrayuint("trsize.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);


	///////////// CONNECTED COMPONENTS LABELING ///////////////////
	kernel_initLabels_ccl_improved<<<n_blocks,n_threads>>>(d_trlabels, n_patches);

	///debug
	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trlabels_init.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	//debug
	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//maxSz = 0, minSz = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	//cout << "Max in trsize: " << maxSz << endl;
	//cout << "Min in trsize: " << minSz << endl;
	//writePPMGrayuint("trsize_init.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);
		
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
	  kernel_ccl_mesh_B_improved<<<dim3(WIDTH/SIZE_PATCH/n_threads_x_ccl, HEIGHT/SIZE_PATCH/n_threads_y_ccl, 1), dim3(n_threads_x_ccl,n_threads_y_ccl,1), n_threads_x_ccl*n_threads_y_ccl*sizeof(unsigned int)>>>(d_trlabels, d_trsize, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      checkCudaErrors(cudaDeviceSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	//debug
	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trlabels_ccl.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	
	//debug
	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//maxSz = 0, minSz = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	//cout << "Max in trsize: " << maxSz << endl;
	//cout << "Min in trsize: " << minSz << endl;
	//writePPMGrayuint("trsize_ccl.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);

	kernel_findmax<<< 1, 512 >>>(d_trsize, d_max, d_max_gid, n_patches);

	//debug
	//checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//cout << "Max : " << *ntr << endl;

	kernel_filterbiggestblob<<<n_blocks, n_threads >>>(d_trlabels, d_max_gid, n_patches);


	//debug
	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trlabels_blob.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);

	//debug
	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//maxSz = 0, minSz = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	//cout << "Max in trsize: " << maxSz << endl;
	//cout << "Min in trsize: " << minSz << endl;
	//writePPMGrayuint("trsize_blob.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);



	//2*blocksize must be a power of two and n_patches should be dividable to 2*blocksize
	kernel_prefixsumscan_compact_uint<<< 1, 32, 64 * sizeof(unsigned int) >>>(d_trcompact, d_trlabels, n_patches);

	
	//debug
	//checkCudaErrors(cudaMemcpy(trlabels, d_trcompact, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trcompact_blob.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	

	kernel_insert_training_samples_w_inds<<< n_blocks, n_threads>>>(d_trlabels, d_trcompact, d_hists, d_trbuffer, d_trinds, d_max, n_patches);

	checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//cout << "Number of training samples : " << *ntr << endl;

	if (*ntr > 0)
	{
		if (isFirstFrame)
		{
			/// EXPECTATION MAXIMIZATION ///
			insertRandIndices(iRand, ntr);
			checkCudaErrors(cudaMemcpy(d_irand, iRand, mem_size_irand, cudaMemcpyHostToDevice));
	
			int pow = 0, pow_n_bins_sqr = 0;
			for (int i = *ntr; i > 1; i>>=1)
				pow++;
			for (int i = N_BINS_SQR; i > 1; i>>=1)
				pow_n_bins_sqr++;
			unsigned int nthreads_em = 1 << pow;
			unsigned int nthreads_em_cnsts = 1 << pow_n_bins_sqr;
			nthreads_em = (nthreads_em > 256) ? 256 : nthreads_em;
			nthreads_em_cnsts = (nthreads_em_cnsts > 256) ? 256 : nthreads_em_cnsts;
			n_blocks_em = (nthreads_em <= 256) ? 1 : n_blocks_em;
	
			float likelihood_change = INF;
			float likelihood_old = 0.0f;
			unsigned int it = 0;
	
			while (likelihood_change > term_criterion_em && it < 100)
			{
				bool isFirstIt = false;
				if (it == 0)
					isFirstIt = true;
				kernel_expectation_step_1<<<dim3(n_blocks_em,N_CLUSTERS_INITIAL,1), nthreads_em>>>(d_trbuffer, d_means, d_invCovs, d_weights, d_probs, d_irand, d_max, isFirstIt);

				float* d_likelihood;
				checkCudaErrors(cudaMalloc((void **) &d_likelihood, n_blocks_em * sizeof(float)));
				kernel_expectation_step_2<<<n_blocks_em, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_likelihood, d_max);
				checkCudaErrors(cudaMemcpy(likelihood, d_likelihood, n_blocks_em * sizeof(float), cudaMemcpyDeviceToHost));
				checkCudaErrors(cudaFree(d_likelihood));
				for (unsigned int j = 1; j < n_blocks_em; j++)
					likelihood[0] += likelihood[j];
				likelihood_change = abs(likelihood[0] - likelihood_old);
				likelihood_old = likelihood[0];	

				kernel_maximization_step_n<<<N_CLUSTERS_INITIAL, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_n, d_weights, d_max);
				kernel_maximization_step_means<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trbuffer, d_probs, d_means, d_n, d_max);
				kernel_maximization_step_covariance<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trbuffer, d_probs, d_means, d_invCovs, d_n, d_max, INF, COV_MIN);
				it++;
			}
			*n_models += N_CLUSTERS_INITIAL;
		}
		else
		{
			int pow = 0, pow_n_bins_sqr = 0;
			for (int i = *ntr; i > 1; i>>=1)
				pow++;
			unsigned int nthreads_update = 1 << pow;
			nthreads_update = (nthreads_update > 256) ? 256 : nthreads_update;
			n_blocks_em = (nthreads_update <= 256) ? 1 : n_blocks_em;

			unsigned int *ntr_old = new unsigned int[1];
			*ntr_old = *ntr;

			/// MODEL UPDATE ///
			kernel_update_models_step1<<<n_blocks_em, nthreads_update>>>(d_trbuffer, d_trinds, d_means, d_invCovs, d_weights, *n_models, THRESHOLD_UPDATE_MAH, INF, d_max);
			checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
			checkCudaErrors(cudaMemcpy(d_max_old, ntr_old, sizeof(unsigned int), cudaMemcpyHostToDevice));

			if (*ntr != *ntr_old)
			{
				int mpow = 0;
				for (int i = *n_models * N_BINS_SQR; i > 1; i>>=1)
					mpow++;
				unsigned int nthreads_update2 = 1 << mpow;
				nthreads_update2 = (nthreads_update2 > 256) ? 256 : nthreads_update2;
				n_blocks_em = (nthreads_update2 <= 256) ? 1 : n_blocks_em;

				kernel_update_models_step2<<<n_blocks_em, nthreads_update2>>>(d_means, d_invCovs, d_weights, *n_models, COV_MIN, INF);
				checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));
				float sum_weights = 0.0f;
				for (int i = 0; i < *n_models; i++)
					sum_weights += weights[i];
				for (int i = 0; i < *n_models; i++)
					weights[i] /= sum_weights;

				checkCudaErrors(cudaMemcpy(d_weights, weights, mem_size_weights, cudaMemcpyHostToDevice));
			}
			else
				checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));
			
			if (*ntr > 0)
			{
				kernel_prefixsumscan_compact_uint_nontex<<< 1, 32, 64 * sizeof(unsigned int) >>>(d_trcompact, d_trinds, n_patches);
				kernel_insert_remaining_training_samples<<< n_blocks, n_threads>>>(d_trinds, d_trcompact, d_trbuffer, d_trremaining, d_max, d_max_old, n_patches);
				
				checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
				checkCudaErrors(cudaMemcpy(d_max_old, ntr_old, sizeof(unsigned int), cudaMemcpyHostToDevice));

				pow = 0;			
				for (int i = *ntr; i > 1; i>>=1)
					pow++;
				for (int i = N_BINS_SQR; i > 1; i>>=1)
					pow_n_bins_sqr++;
				unsigned int nthreads_em = 1 << pow;
				unsigned int nthreads_em_cnsts = 1 << pow_n_bins_sqr;
				nthreads_em = (nthreads_em > 256) ? 256 : nthreads_em;
				nthreads_em_cnsts = (nthreads_em_cnsts > 256) ? 256 : nthreads_em_cnsts;
				n_blocks_em = (nthreads_em <= 256) ? 1 : n_blocks_em;

				
				/// EXPECTATION MAXIMIZATION ///
				insertRandIndices(iRand, ntr);
				checkCudaErrors(cudaMemcpy(d_irand, iRand, mem_size_irand, cudaMemcpyHostToDevice));

				float likelihood_change = INF;
				float likelihood_old = 0.0f;
				unsigned int it = 0;
		
				while (likelihood_change > term_criterion_em && it < 100)
				{
					bool isFirstIt = false;
					if (it == 0)
						isFirstIt = true;
					kernel_expectation_step_1<<<dim3(n_blocks_em,N_CLUSTERS_NEW,1), nthreads_em>>>(d_trremaining, d_means_new, d_invCovs_new, d_weights_new, d_probs_new, d_irand, d_max, isFirstIt);

					float* d_likelihood;
					checkCudaErrors(cudaMalloc((void **) &d_likelihood, n_blocks_em * sizeof(float)));
					kernel_expectation_step_2<<<n_blocks_em, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs_new, d_likelihood, d_max);
					checkCudaErrors(cudaMemcpy(likelihood, d_likelihood, n_blocks_em * sizeof(float), cudaMemcpyDeviceToHost));
					checkCudaErrors(cudaFree(d_likelihood));
					for (unsigned int j = 1; j < n_blocks_em; j++)
						likelihood[0] += likelihood[j];
					likelihood_change = abs(likelihood[0] - likelihood_old);
					likelihood_old = likelihood[0];	
	
					kernel_maximization_step_n<<<N_CLUSTERS_NEW, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs_new, d_n_new, d_weights_new, d_max);
					kernel_maximization_step_means<<<dim3(N_CLUSTERS_NEW, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trremaining, d_probs_new, d_means_new, d_n_new, d_max);
					kernel_maximization_step_covariance<<<dim3(N_CLUSTERS_NEW, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trremaining, d_probs_new, d_means_new, d_invCovs_new, d_n_new, d_max, INF, COV_MIN);
					it++;
				}
				checkCudaErrors(cudaMemcpy(weights_new, d_weights_new, mem_size_weights_new, cudaMemcpyDeviceToHost));
				float scale = (float)(*ntr) / (float)(*ntr_old);
				for (int i = 0; i < N_CLUSTERS_NEW; i++)
					weights_new[i] *= scale;
				checkCudaErrors(cudaMemcpy(d_weights_new, weights_new, mem_size_weights_new, cudaMemcpyHostToDevice));

				//// INSERT NEW LEARNED MODELS TO MODEL LIBRARY ////
				unsigned int *ind_clusters_min_weights = new unsigned int[N_CLUSTERS_MAX];
				unsigned int n_clusters_replaced = N_CLUSTERS_NEW;
				
				if (*n_models > N_CLUSTERS_MAX - N_CLUSTERS_NEW)
				{
					n_clusters_replaced = *n_models - (N_CLUSTERS_MAX - N_CLUSTERS_NEW);
					for (int i = 0; i < n_clusters_replaced; i++)
					{
						float weight_min = INF;
						unsigned int ind_weight_min = 0;
						for (int j = 0; j < *n_models; j++)
						{
							ind_weight_min = (weights[j] < weight_min) ? j : ind_weight_min; 
							weight_min = (weights[j] < weight_min) ? weights[j] : weight_min; 
						}
						weights[ind_weight_min] = INF;
						ind_clusters_min_weights[i] = ind_weight_min;
					}
					for (int i = n_clusters_replaced; i < N_CLUSTERS_NEW; i++)
						ind_clusters_min_weights[i] = i + *n_models;
					*n_models += N_CLUSTERS_NEW - n_clusters_replaced;
				}
				else
				{
					for (int i = 0; i < N_CLUSTERS_NEW; i++)
						ind_clusters_min_weights[i] = i + *n_models;
					*n_models += N_CLUSTERS_NEW;
				}

				checkCudaErrors(cudaMemcpy(d_ind_clusters_min_weights, ind_clusters_min_weights, N_CLUSTERS_MAX * sizeof(unsigned int), cudaMemcpyHostToDevice));
				kernel_replace_models<<<n_clusters_replaced, N_BINS_SQR>>>(d_means, d_invCovs, d_weights, d_means_new, d_invCovs_new, d_weights_new, d_ind_clusters_min_weights);

				checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));
				float sum_weights_2 = 0.0f;
				for (int i = 0; i < *n_models; i++)
					sum_weights_2 += weights[i];
				for (int i = 0; i < *n_models; i++)
					weights[i] /= sum_weights_2;
				checkCudaErrors(cudaMemcpy(d_weights, weights, mem_size_weights, cudaMemcpyHostToDevice));
			}
		}
	}
	///// CLASSIFICATION STEPS /////
	kernel_classification_raw<<<n_blocks, n_threads/2>>>(d_hists, d_classified, d_trlabels, d_means, d_invCovs, n_patches, *n_models, THRESHOLD_CLASSIFY_MAH, INF);
	kernel_initLabels_ccl_improved<<<n_blocks,n_threads>>>(d_classified, n_patches);
	m_IsNotDone = 1;
	ite = 0;
	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
	  kernel_ccl_mesh_B_improved_classify<<<dim3(WIDTH/SIZE_PATCH/n_threads_x_ccl, HEIGHT/SIZE_PATCH/n_threads_y_ccl, 1), dim3(n_threads_x_ccl,n_threads_y_ccl,1), n_threads_x_ccl*n_threads_y_ccl*sizeof(unsigned int)>>>(d_classified, d_roadlabel, d_max_gid, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      checkCudaErrors(cudaDeviceSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	kernel_filter<<<n_blocks, n_threads >>>(d_classified, d_roadlabel, n_patches);
	
    // check if kernel execution generated an error
    getLastCudaError("Kernel execution failed");

	//get the classified image from the device
	checkCudaErrors(cudaMemcpy(classified, d_classified, mem_size_classified, cudaMemcpyDeviceToHost));
    
    checkCudaErrors(cudaEventRecord(stop, 0));
    // make sure GPU has finished copying
    checkCudaErrors(cudaDeviceSynchronize());
    //get the the total elapsed time in ms
    sdkStopTimer(&timer);
    checkCudaErrors(cudaEventElapsedTime(&elapsedTimeInMs, start, stop));
    elapsedTimeInMs = sdkGetTimerValue(&timer);

    sdkDeleteTimer(&timer);

	checkCudaErrors(cudaEventDestroy(stop));
    checkCudaErrors(cudaEventDestroy(start));
    
    return elapsedTimeInMs;
}

////////////////////////////////////////////////////////////////////////////////
//! Entry point for Cuda functionality on host side
//! @param argc  command line argument count
//! @param argv  command line arguments
//! @param data  data to process on the device
//! @param len   len of \a data
////////////////////////////////////////////////////////////////////////////////
extern "C" float
detectRoadDebug(const int argc, const char **argv, float4 *data, unsigned int *n_models, bool isFirstFrame, unsigned int *classified)
{
	//number of pixels
	unsigned int n_pix = WIDTH * HEIGHT;
	//number of patches
	unsigned int n_patches = n_pix / SIZE_PATCH / SIZE_PATCH; 
	//number of total histogram bins
	unsigned int n_hists = n_patches * N_BINS_SQR;
	
	/// EM STUFF ///
	//number of blocks to be used in em algorithm
	unsigned int n_blocks_em = 2;
	//number of training samples
	unsigned int *ntr = new unsigned int[1];
	*ntr = 0;
	//random indices
	unsigned int *iRand = new unsigned int[N_CLUSTERS_MAX];
	//likelihood array
	float *likelihood = new float[n_blocks_em];
	//temporary model weight array
	float *weights = new float[N_CLUSTERS_MAX];
	float *weights_new = new float[N_CLUSTERS_NEW];

	////// CCL STUFF //////
	unsigned int m_IsNotDone = 1;
	unsigned int ite = 0;
	

	//gpu timer stuff
	StopWatchInterface *timer = NULL;
    float elapsedTimeInMs = 0.0f;
    cudaEvent_t start, stop;
    sdkCreateTimer(&timer);
    checkCudaErrors(cudaEventCreate(&start));
    checkCudaErrors(cudaEventCreate(&stop));

	//get the device properties and set number of blocks according to that
	cudaDeviceProp prop;
	checkCudaErrors( cudaGetDeviceProperties( &prop, 0 ) );
	int blocks = prop.multiProcessorCount;

	checkCudaErrors(cudaMemset( d_ghist, 0, mem_size_ghist));
	checkCudaErrors(cudaMemset( d_cdf, 0, mem_size_cdf));
	checkCudaErrors(cudaMemset( d_trlabels, 0, mem_size_trlabels));
	checkCudaErrors(cudaMemset( d_trinds, 0, mem_size_trlabels));
	checkCudaErrors(cudaMemset( d_trsize, 0, mem_size_trsize));
	checkCudaErrors(cudaMemset( d_ntr, 0, sizeof(unsigned int)));
	checkCudaErrors(cudaMemset( d_max, 0, sizeof(unsigned int)));

    //start the timer
    sdkStartTimer(&timer);
    checkCudaErrors(cudaEventRecord(start, 0));
    
    // copy image data to device
    checkCudaErrors(cudaMemcpy(d_data, data, mem_size_data, cudaMemcpyHostToDevice));
    
	//////////////// ALGORITHM BEGINS ////////////////

	///////////////// PRE-PROCESSING //////////////////
	kernel_hist_tex<<< blocks*4, 256 >>>(d_ghist, n_pix);
	kernel_prefixsumscan_cdf<<< 1, 256/2>>>(d_cdf, d_ghist, 256, n_pix);
	kernel_findmin<<<1, 256, sizeof(unsigned int)*3*256>>>(d_cdf, d_min, 256);
	kernel_histeq<<< blocks*4, 256 >>>(d_data, d_min, 256, n_pix);
	kernel_rgb2hsv_2<<< blocks*15, 320 >>>(d_data, n_pix);
	kernel_downsample_4_trlocs_uint<<< blocks*15, 320 >>>(d_data, d_hists, d_trlabels, d_trsize, d_ntr, WIDTH, n_patches, SIZE_PATCH, THRESHOLD_VAR_Y);
	
	/*
	unsigned int *trlabels = new unsigned int[n_patches];
	unsigned int *trsize = new unsigned int[n_patches];
	checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	float maxLbl = 0, minLbl = INF;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	cout << "Max in trlabels: " << maxLbl << endl;
	cout << "Min in trlabels: " << minLbl << endl;
	writePPMGrayuint("trlabels.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);

	checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	float maxSz = 0, minSz = INF;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	cout << "Max in trsize: " << maxSz << endl;
	cout << "Min in trsize: " << minSz << endl;
	writePPMGrayuint("trsize.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);

	
	//checkCudaErrors(cudaMemcpy(ntr, d_ntr, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//cout << "Number of training samples : " << *ntr << endl;
	*/


	///////////// CONNECTED COMPONENTS LABELING ///////////////////
	kernel_initLabels_ccl_improved<<<blocks*15,320>>>(d_trlabels, n_patches);
	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trlabels_init.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);

	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//maxSz = 0, minSz = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	//cout << "Max in trsize: " << maxSz << endl;
	//cout << "Min in trsize: " << minSz << endl;
	//writePPMGrayuint("trsize_init.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);
	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
	  kernel_ccl_mesh_B_improved<<<dim3(WIDTH/SIZE_PATCH/16, HEIGHT/SIZE_PATCH/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_trlabels, d_trsize, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      checkCudaErrors(cudaDeviceSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//maxLbl = 0, minLbl = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	//cout << "Max in trlabels: " << maxLbl << endl;
	//cout << "Min in trlabels: " << minLbl << endl;
	//writePPMGrayuint("trlabels_ccl.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);

	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//maxSz = 0, minSz = INF;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
	//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	//cout << "Max in trsize: " << maxSz << endl;
	//cout << "Min in trsize: " << minSz << endl;
	//writePPMGrayuint("trsize_ccl.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);

	kernel_findmax<<< 1, 64 >>>(d_trsize, d_max, d_max_gid, n_patches);
	//checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//cout << "Max : " << *ntr << endl;
	kernel_filterbiggestblob<<<blocks * 3, 320 >>>(d_trlabels, d_max_gid, n_patches);

	/*
	checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	maxLbl = 0, minLbl = INF;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	cout << "Max in trlabels: " << maxLbl << endl;
	cout << "Min in trlabels: " << minLbl << endl;
	writePPMGrayuint("trlabels_blob.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	*/

	/*
	checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	maxSz = 0, minSz = INF;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
	cout << "Max in trsize: " << maxSz << endl;
	cout << "Min in trsize: " << minSz << endl;
	writePPMGrayuint("trsize_blob.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);
	//*/


	//2*blocksize must be a power of two and n_patches should be dividable to 2*blocksize
	kernel_prefixsumscan_compact_uint<<< 1, 32, 64 * sizeof(unsigned int) >>>(d_trcompact, d_trlabels, n_patches);

	/*
	checkCudaErrors(cudaMemcpy(trlabels, d_trcompact, mem_size_trlabels, cudaMemcpyDeviceToHost));
	maxLbl = 0, minLbl = INF;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	cout << "Max in trlabels: " << maxLbl << endl;
	cout << "Min in trlabels: " << minLbl << endl;
	writePPMGrayuint("trcompact_blob.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	//*/

	kernel_insert_training_samples_w_inds<<< blocks * 3, 320>>>(d_trlabels, d_trcompact, d_hists, d_trbuffer, d_trinds, d_max, n_patches);

	/*	
	checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	maxLbl = 0, minLbl = INF;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		maxLbl = (trlabels[i] > maxLbl) ? trlabels[i] : maxLbl;
	for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
		minLbl = (trlabels[i] < minLbl) ? trlabels[i] : minLbl;
	cout << "Max in trlabels: " << maxLbl << endl;
	cout << "Min in trlabels: " << minLbl << endl;
	writePPMGrayuint("trlabels_blob.ppm", trlabels, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxLbl);
	//*/

	checkCudaErrors(cudaDeviceSynchronize());
	checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//cout << "Number of training samples : " << *ntr << endl;

	if (*ntr > 0)
	{
		if (isFirstFrame)
		{
			/// EXPECTATION MAXIMIZATION ///
			insertRandIndices(iRand, ntr);
			checkCudaErrors(cudaMemcpy(d_irand, iRand, mem_size_irand, cudaMemcpyHostToDevice));
	
			int pow = 0, pow_n_bins_sqr = 0;
			for (int i = *ntr; i > 1; i>>=1)
				pow++;
			for (int i = N_BINS_SQR; i > 1; i>>=1)
				pow_n_bins_sqr++;
			unsigned int nthreads_em = 1 << pow;
			unsigned int nthreads_em_cnsts = 1 << pow_n_bins_sqr;
			nthreads_em = (nthreads_em > 256) ? 256 : nthreads_em;
			nthreads_em_cnsts = (nthreads_em_cnsts > 256) ? 256 : nthreads_em_cnsts;
			n_blocks_em = (nthreads_em <= 256) ? 1 : n_blocks_em;
	
			float likelihood_change = INF;
			float likelihood_old = 0.0f;
			unsigned int it = 0;
	
			while (likelihood_change > 0.1 && it < 100)
			{
				bool isFirstIt = false;
				if (it == 0)
					isFirstIt = true;
				kernel_expectation_step_1<<<dim3(n_blocks_em,N_CLUSTERS_INITIAL,1), nthreads_em>>>(d_trbuffer, d_means, d_invCovs, d_weights, d_probs, d_irand, d_max, isFirstIt);
				checkCudaErrors(cudaDeviceSynchronize());

				float* d_likelihood;
				checkCudaErrors(cudaMalloc((void **) &d_likelihood, n_blocks_em * sizeof(float)));
				kernel_expectation_step_2<<<n_blocks_em, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_likelihood, d_max);
				checkCudaErrors(cudaDeviceSynchronize());
				checkCudaErrors(cudaMemcpy(likelihood, d_likelihood, n_blocks_em * sizeof(float), cudaMemcpyDeviceToHost));
				checkCudaErrors(cudaFree(d_likelihood));
				for (unsigned int j = 1; j < n_blocks_em; j++)
					likelihood[0] += likelihood[j];
				likelihood_change = abs(likelihood[0] - likelihood_old);
				likelihood_old = likelihood[0];	

				kernel_maximization_step_n<<<N_CLUSTERS_INITIAL, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_n, d_weights, d_max);
				kernel_maximization_step_means<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trbuffer, d_probs, d_means, d_n, d_max);
				kernel_maximization_step_covariance<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trbuffer, d_probs, d_means, d_invCovs, d_n, d_max, INF, COV_MIN);
				it++;
			}
			*n_models += N_CLUSTERS_INITIAL;
			//cout << "Number of EM Iterations :" << it << endl;
			//cout << "Number of models :" << *n_models << endl;
		}
		else
		{
			int pow = 0, pow_n_bins_sqr = 0;
			for (int i = *ntr; i > 1; i>>=1)
				pow++;
			unsigned int nthreads_update = 1 << pow;
			nthreads_update = (nthreads_update > 256) ? 256 : nthreads_update;
			n_blocks_em = (nthreads_update <= 256) ? 1 : n_blocks_em;

			//cout << "Number of samples before update :" << *ntr << endl;
			unsigned int *ntr_old = new unsigned int[1];
			*ntr_old = *ntr;

			/*
			float *means = new float [N_CLUSTERS_MAX * N_BINS_SQR];
			checkCudaErrors(cudaMemcpy(means, d_means, mem_size_means, cudaMemcpyDeviceToHost));
			for (int i = 0; i < *n_models; i++)
			{
				cout << "Means before " << i << " : ";
				for(int j = 0; j < N_BINS_SQR; j++)
					cout << means[i * N_BINS_SQR + j] << " ";
			}
			cout << endl << endl;
			float *invCovs = new float [N_CLUSTERS_MAX * N_BINS_SQR];
			checkCudaErrors(cudaMemcpy(invCovs, d_invCovs, mem_size_invCovs, cudaMemcpyDeviceToHost));
			for (int i = 0; i < *n_models; i++)
			{
				cout << "invCov before " << i << " : ";
				for(int j = 0; j < N_BINS_SQR; j++)
					cout << invCovs[i * N_BINS_SQR + j] << " ";
			}
			cout << endl << endl;
			checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));				
			cout << "Weights before update : ";
			for (int i = 0; i < *n_models; i++)
				cout << weights[i];
			cout << endl << endl;
			*/

			/// MODEL UPDATE ///
			checkCudaErrors(cudaDeviceSynchronize());
			kernel_update_models_step1<<<n_blocks_em, nthreads_update>>>(d_trbuffer, d_trinds, d_means, d_invCovs, d_weights, *n_models, THRESHOLD_UPDATE_MAH, INF, d_max);
			//checkCudaErrors(cudaThreadSynchronize());
			checkCudaErrors(cudaDeviceSynchronize());

			/*
			checkCudaErrors(cudaMemcpy(means, d_means, mem_size_means, cudaMemcpyDeviceToHost));
			for (int i = 0; i < *n_models; i++)
			{
				cout << "Means after " << i << " : ";
				for(int j = 0; j < N_BINS_SQR; j++)
					cout << means[i * N_BINS_SQR + j] << " ";
			}
			cout << endl << endl;
			checkCudaErrors(cudaMemcpy(invCovs, d_invCovs, mem_size_invCovs, cudaMemcpyDeviceToHost));
			for (int i = 0; i < *n_models; i++)
			{
				cout << "invCov after " << i << " : ";
				for(int j = 0; j < N_BINS_SQR; j++)
					cout << invCovs[i * N_BINS_SQR + j] << " ";
			}
			cout << endl << endl;
			checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));				
			cout << "Weights after update : ";
			for (int i = 0; i < *n_models; i++)
				cout << weights[i];
			cout << endl << endl;
			*/

			checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
			checkCudaErrors(cudaMemcpy(d_max_old, ntr_old, sizeof(unsigned int), cudaMemcpyHostToDevice));
			//cout << "Number of samples after update :" << *ntr << endl;

			//if (*ntr != *ntr_old)
			//{
				int mpow = 0;
				for (int i = *n_models * N_BINS_SQR; i > 1; i>>=1)
					mpow++;
				unsigned int nthreads_update2 = 1 << mpow;
				nthreads_update2 = (nthreads_update2 > 256) ? 256 : nthreads_update2;
				n_blocks_em = (nthreads_update2 <= 256) ? 1 : n_blocks_em;

				kernel_update_models_step2<<<n_blocks_em, nthreads_update2>>>(d_means, d_invCovs, d_weights, *n_models, COV_MIN, INF);
				checkCudaErrors(cudaDeviceSynchronize());
				checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));
				float sum_weights = 0.0f;
				for (int i = 0; i < *n_models; i++)
					sum_weights += weights[i];
				for (int i = 0; i < *n_models; i++)
					weights[i] /= sum_weights;

				/*
				cout << "Weights after after update : ";
				for (int i = 0; i < *n_models; i++)
					cout << weights[i];
				cout << endl << endl;
				*/	

				checkCudaErrors(cudaMemcpy(d_weights, weights, mem_size_weights, cudaMemcpyHostToDevice));
				//cout << "Sum of the new weights :" << sum_weights << endl;
			//}
			//else
			//	checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));

			/*
			checkCudaErrors(cudaMemcpy(means, d_means, mem_size_means, cudaMemcpyDeviceToHost));
			for (int i = 0; i < *n_models; i++)
			{
				cout << "Means after after " << i << " : ";
				for(int j = 0; j < N_BINS_SQR; j++)
					cout << means[i * N_BINS_SQR + j] << " ";
			}
			cout << endl << endl;
			checkCudaErrors(cudaMemcpy(invCovs, d_invCovs, mem_size_invCovs, cudaMemcpyDeviceToHost));
			for (int i = 0; i < *n_models; i++)
			{
				cout << "invCov after after " << i << " : ";
				for(int j = 0; j < N_BINS_SQR; j++)
					cout << invCovs[i * N_BINS_SQR + j] << " ";
			}
			cout << endl << endl;
			*/

			//checkCudaErrors(cudaMemcpy(means, d_means, mem_size_means, cudaMemcpyDeviceToHost));
			//for (int i = 0; i < N_CLUSTERS_NEW * N_BINS_SQR; i++)
			//	cout << "Mean " << i / N_BINS_SQR << " : " << means[i] << endl;
			//cout << endl;
			//checkCudaErrors(cudaMemcpy(invCovs, d_invCovs, mem_size_invCovs, cudaMemcpyDeviceToHost));
			//for (int i = 0; i < *n_models; i++)
			//{
			//	cout << "invCov " << i / N_BINS_SQR << " : ";
			//	for(int j = 0; j < N_BINS_SQR; j++)
			//		cout << invCovs[i * N_BINS_SQR + j] << " ";
			//}
			//cout << endl << endl;

			
			if (*ntr > 0)
			{
				kernel_prefixsumscan_compact_uint_nontex<<< 1, 32, 64 * sizeof(unsigned int) >>>(d_trcompact, d_trinds, n_patches);
				
				//checkCudaErrors(cudaMemcpy(trsize, d_trcompact, mem_size_trcompact, cudaMemcpyDeviceToHost));
				//maxSz = 0, minSz = INF;
				//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
				//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
				//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
				//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
				//cout << "Max in trcomp: " << maxSz << endl;
				//cout << "Min in trcomp: " << minSz << endl;
				//writePPMGrayuint("trcomp.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);
				//checkCudaErrors(cudaMemcpy(trsize, d_trinds, mem_size_trsize, cudaMemcpyDeviceToHost));
				//maxSz = 0, minSz = INF;
				//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
				//	maxSz = (trsize[i] > maxSz) ? trsize[i] : maxSz;
				//for (int i = 0; i < (WIDTH/SIZE_PATCH) * (HEIGHT/SIZE_PATCH); i++)
				//	minSz = (trsize[i] < minSz) ? trsize[i] : minSz;
				//cout << "Max in trinds: " << maxSz << endl;
				//cout << "Min in trinds: " << minSz << endl;
				//writePPMGrayuint("trinds.ppm", trsize, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH, 255.0f/maxSz);
				

				
				kernel_insert_remaining_training_samples<<< blocks * 3, 320>>>(d_trinds, d_trcompact, d_trbuffer, d_trremaining, d_max, d_max_old, n_patches);

				checkCudaErrors(cudaDeviceSynchronize());
				checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
				checkCudaErrors(cudaMemcpy(d_max_old, ntr_old, sizeof(unsigned int), cudaMemcpyHostToDevice));
				//cout << "Number of samples after after update :" << *ntr << endl;

				pow = 0;			
				for (int i = *ntr; i > 1; i>>=1)
					pow++;
				for (int i = N_BINS_SQR; i > 1; i>>=1)
					pow_n_bins_sqr++;
				unsigned int nthreads_em = 1 << pow;
				unsigned int nthreads_em_cnsts = 1 << pow_n_bins_sqr;
				nthreads_em = (nthreads_em > 256) ? 256 : nthreads_em;
				nthreads_em_cnsts = (nthreads_em_cnsts > 256) ? 256 : nthreads_em_cnsts;
				n_blocks_em = (nthreads_em <= 256) ? 1 : n_blocks_em;

				
				/// EXPECTATION MAXIMIZATION ///
				insertRandIndices(iRand, ntr);
				checkCudaErrors(cudaMemcpy(d_irand, iRand, mem_size_irand, cudaMemcpyHostToDevice));

				float likelihood_change = INF;
				float likelihood_old = 0.0f;
				unsigned int it = 0;
		
				while (likelihood_change > 0.1 && it < 100)
				{
					bool isFirstIt = false;
					if (it == 0)
						isFirstIt = true;
					kernel_expectation_step_1<<<dim3(n_blocks_em,N_CLUSTERS_NEW,1), nthreads_em>>>(d_trremaining, d_means_new, d_invCovs_new, d_weights_new, d_probs_new, d_irand, d_max, isFirstIt);
					checkCudaErrors(cudaDeviceSynchronize());

					float* d_likelihood;
					checkCudaErrors(cudaMalloc((void **) &d_likelihood, n_blocks_em * sizeof(float)));
					kernel_expectation_step_2<<<n_blocks_em, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs_new, d_likelihood, d_max);
					checkCudaErrors(cudaDeviceSynchronize());
					checkCudaErrors(cudaMemcpy(likelihood, d_likelihood, n_blocks_em * sizeof(float), cudaMemcpyDeviceToHost));
					checkCudaErrors(cudaFree(d_likelihood));
					for (unsigned int j = 1; j < n_blocks_em; j++)
						likelihood[0] += likelihood[j];
					likelihood_change = abs(likelihood[0] - likelihood_old);
					likelihood_old = likelihood[0];	
	
					kernel_maximization_step_n<<<N_CLUSTERS_NEW, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs_new, d_n_new, d_weights_new, d_max);
					kernel_maximization_step_means<<<dim3(N_CLUSTERS_NEW, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trremaining, d_probs_new, d_means_new, d_n_new, d_max);
					kernel_maximization_step_covariance<<<dim3(N_CLUSTERS_NEW, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_trremaining, d_probs_new, d_means_new, d_invCovs_new, d_n_new, d_max, INF, COV_MIN);
					it++;
				}
				checkCudaErrors(cudaDeviceSynchronize());
				checkCudaErrors(cudaMemcpy(weights_new, d_weights_new, mem_size_weights_new, cudaMemcpyDeviceToHost));
				float scale = (float)(*ntr) / (float)(*ntr_old);
				//cout << "Scale : " << scale << endl;
				for (int i = 0; i < N_CLUSTERS_NEW; i++)
					weights_new[i] *= scale;
				checkCudaErrors(cudaMemcpy(d_weights_new, weights_new, mem_size_weights_new, cudaMemcpyHostToDevice));

				
				//checkCudaErrors(cudaMemcpy(means, d_means_new, mem_size_means_new, cudaMemcpyDeviceToHost));
				//for (int i = 0; i < *n_models; i++)
				//{
				//	cout << "Means " << i / N_BINS_SQR << " : ";
				//	for(int j = 0; j < N_BINS_SQR; j++)
				//		cout << means[i * N_BINS_SQR + j] << " ";
				//}
				//cout << endl << endl;
				//for (int i = 0; i < *n_models; i++)
				//{
				//	cout << "Weights " << i << " : " << weights_new[i];
				//}
				//cout << endl << endl;

				//checkCudaErrors(cudaMemcpy(invCovs, d_invCovs_new, mem_size_invCovs_new, cudaMemcpyDeviceToHost));
				//for (int i = 0; i < *n_models; i++)
				//{
				//	cout << "invCov " << i / N_BINS_SQR << " : ";
				//	for(int j = 0; j < N_BINS_SQR; j++)
				//		cout << invCovs[i * N_BINS_SQR + j] << " ";
				//}
				//cout << endl << endl;

				//// INSERT NEW LEARNED MODELS TO MODEL LIBRARY ////
				unsigned int *ind_clusters_min_weights = new unsigned int[N_CLUSTERS_MAX];
				unsigned int n_clusters_replaced = N_CLUSTERS_NEW;
				
				if (*n_models > N_CLUSTERS_MAX - N_CLUSTERS_NEW)
				{
					n_clusters_replaced = *n_models - (N_CLUSTERS_MAX - N_CLUSTERS_NEW);
					for (int i = 0; i < n_clusters_replaced; i++)
					{
						float weight_min = INF;
						unsigned int ind_weight_min = 0;
						for (int j = 0; j < *n_models; j++)
						{
							ind_weight_min = (weights[j] < weight_min) ? j : ind_weight_min; 
							weight_min = (weights[j] < weight_min) ? weights[j] : weight_min; 
						}
						weights[ind_weight_min] = INF;
						ind_clusters_min_weights[i] = ind_weight_min;
					}
					for (int i = n_clusters_replaced; i < N_CLUSTERS_NEW; i++)
						ind_clusters_min_weights[i] = i + *n_models;
					*n_models += N_CLUSTERS_NEW - n_clusters_replaced;
				}
				else
				{
					for (int i = 0; i < N_CLUSTERS_NEW; i++)
						ind_clusters_min_weights[i] = i + *n_models;
					*n_models += N_CLUSTERS_NEW;
				}

				checkCudaErrors(cudaMemcpy(d_ind_clusters_min_weights, ind_clusters_min_weights, N_CLUSTERS_MAX * sizeof(unsigned int), cudaMemcpyHostToDevice));
				kernel_replace_models<<<n_clusters_replaced, N_BINS_SQR>>>(d_means, d_invCovs, d_weights, d_means_new, d_invCovs_new, d_weights_new, d_ind_clusters_min_weights);

				checkCudaErrors(cudaDeviceSynchronize());
				checkCudaErrors(cudaMemcpy(weights, d_weights, mem_size_weights, cudaMemcpyDeviceToHost));
				float sum_weights_2 = 0.0f;
				for (int i = 0; i < *n_models; i++)
					sum_weights_2 += weights[i];
				for (int i = 0; i < *n_models; i++)
					weights[i] /= sum_weights_2;
				checkCudaErrors(cudaMemcpy(d_weights, weights, mem_size_weights, cudaMemcpyHostToDevice));
				
				//cout << "Number of EM Iterations :" << it << endl;
				
				//cout << "Number of models :" << *n_models << endl;
			}
			
			//cout << "Number of models :" << *n_models << endl;
		}
	}
	///// CLASSIFICATION STEPS /////
	kernel_classification_raw<<<blocks*5, 160>>>(d_hists, d_classified, d_trlabels, d_means, d_invCovs, n_patches, *n_models, THRESHOLD_CLASSIFY_MAH, INF);
	
	
	kernel_initLabels_ccl_improved<<<blocks*15,320>>>(d_classified, n_patches);
	m_IsNotDone = 1;
	ite = 0;
	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
	  kernel_ccl_mesh_B_improved_classify<<<dim3(WIDTH/SIZE_PATCH/16, HEIGHT/SIZE_PATCH/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_classified, d_roadlabel, d_max_gid, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      checkCudaErrors(cudaDeviceSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	kernel_filter<<<blocks * 3, 320 >>>(d_classified, d_roadlabel, n_patches);
	checkCudaErrors(cudaDeviceSynchronize());
	//*/
	
    // check if kernel execution generated and error
    getLastCudaError("Kernel execution failed");

	//get the classified image from the device
	checkCudaErrors(cudaMemcpy(classified, d_classified, mem_size_classified, cudaMemcpyDeviceToHost));
    
    checkCudaErrors(cudaEventRecord(stop, 0));
    // make sure GPU has finished copying
    checkCudaErrors(cudaDeviceSynchronize());
    //get the the total elapsed time in ms
    sdkStopTimer(&timer);
    checkCudaErrors(cudaEventElapsedTime(&elapsedTimeInMs, start, stop));
    elapsedTimeInMs = sdkGetTimerValue(&timer);

    sdkDeleteTimer(&timer);

	checkCudaErrors(cudaEventDestroy(stop));
    checkCudaErrors(cudaEventDestroy(start));
    
    return elapsedTimeInMs;
}

/*
////////////////////////////////////////////////////////////////////////////////
//! Entry point for Cuda functionality on host side
//! @param argc  command line argument count
//! @param argv  command line arguments
//! @param data  data to process on the device
//! @param len   len of \a data
////////////////////////////////////////////////////////////////////////////////
extern "C" float
detectRoadOld(const int argc, const char **argv, float4 *data, unsigned int *n_models, bool isFirstFrame, unsigned int *classified)
{
	//number of blocks to be used in em algorithm
	const unsigned int n_blocks_em = 2;

	//number of pixels
	unsigned int n_pix = WIDTH * HEIGHT;
	//number of 4-element histogram pieces
	unsigned int n_hists = n_pix / SIZE_PATCH / SIZE_PATCH * N_BINS_SQR;
	//number of patches
	unsigned int n_patches = n_pix / SIZE_PATCH / SIZE_PATCH; 
	//number of training samples
	unsigned int *ntr = new unsigned int[1];
	*ntr = 0;
	//random indices
	unsigned int *iRand = new unsigned int[N_CLUSTERS_MAX];
	//likelihood array
	float *likelihood = new float[n_blocks_em];
	
	StopWatchInterface *timer = NULL;
    float elapsedTimeInMs = 0.0f;
    cudaEvent_t start, stop;
    sdkCreateTimer(&timer);
    checkCudaErrors(cudaEventCreate(&start));
    checkCudaErrors(cudaEventCreate(&stop));

	checkCudaErrors(cudaMemset( d_ghist, 0, mem_size_ghist));
	checkCudaErrors(cudaMemset( d_cdf, 0, mem_size_cdf));
	checkCudaErrors(cudaMemset( d_trlabels, 0, mem_size_trlabels));
	checkCudaErrors(cudaMemset( d_trsize, 0, mem_size_trsize));
	checkCudaErrors(cudaMemset( d_ntr, 0, sizeof(unsigned int)));
    
	////// CCL STUFF //////
	unsigned int m_IsNotDone = 1;
	unsigned int ite = 0;

    //start the timer
    sdkStartTimer(&timer);
    checkCudaErrors(cudaEventRecord(start, 0));
    
    // copy image data to device
    checkCudaErrors(cudaMemcpy(d_data, data, mem_size_data, cudaMemcpyHostToDevice));

	cudaDeviceProp prop;
	checkCudaErrors( cudaGetDeviceProperties( &prop, 0 ) );
	int blocks = prop.multiProcessorCount;
    
	//////////////// ALGORITHM BEGINS ////////////////

	///////////////// PRE-PROCESSING //////////////////
	kernel_hist_tex<<< blocks*4, 256 >>>(d_ghist, n_pix);
	kernel_prefixsumscan_cdf<<< 1, 256/2>>>(d_cdf, d_ghist, 256, n_pix);
	kernel_findmin<<<1, 256>>>(d_cdf, d_min);
	kernel_histeq<<< blocks*4, 256 >>>(d_data, d_min, n_pix);
	kernel_rgb2hsv_2<<< blocks*15, 320 >>>(d_data, n_pix);
	kernel_downsample_4_trlocs_uint<<< blocks*15, 320 >>>(d_data, d_hists, d_trlabels, d_trsize, d_ntr, WIDTH, n_patches, SIZE_PATCH, THRESHOLD_VAR_Y);
	
	///////////// CONNECTED COMPONENTS LABELING ///////////////////
	kernel_initLabels_ccl_improved<<<blocks*15,320>>>(d_trlabels, n_patches);
	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
	  kernel_ccl_mesh_B_improved<<<dim3(WIDTH/SIZE_PATCH/16, HEIGHT/SIZE_PATCH/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_trlabels, d_trsize, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      checkCudaErrors(cudaThreadSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	kernel_findmax<<< 1, 64 >>>(d_trsize, d_max, d_maxgid, n_patches);
	kernel_filterbiggestblob<<<blocks * 3, 320 >>>(d_trlabels, d_maxgid, n_patches);
	//2*blocksize must be a power of two and n_patches should be dividable to 2*blocksize
	kernel_prefixsumscan_compact_uint<<< 1, 32, 64 * sizeof(unsigned int) >>>(d_trcompact, d_trlabels, n_patches);
	kernel_insert_training_samples<<< blocks * 3, 320>>>(d_trlabels, d_trcompact, d_hists, d_tr, d_max, n_patches);

	if (isFirstFrame)
	{
		/// EXPECTATION MAXIMIZATION ///
		checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	
		if (ntr > 0)
		{
			insertRandIndices(iRand, ntr);
			checkCudaErrors(cudaMemcpy(d_irand, iRand, mem_size_irand, cudaMemcpyHostToDevice));
	
			int pow = 0, pow_n_bins_sqr = 0;
			for (int i = *ntr; i > 1; i>>=1)
				pow++;
			for (int i = N_BINS_SQR; i > 1; i>>=1)
				pow_n_bins_sqr++;
			unsigned int nthreads_em = 1 << pow;
			unsigned int nthreads_em_cnsts = 1 << pow_n_bins_sqr;
			nthreads_em = (nthreads_em > 256) ? 256 : nthreads_em;
			nthreads_em_cnsts = (nthreads_em_cnsts > 256) ? 256 : nthreads_em_cnsts;

			float likelihood_change = INF;
			float likelihood_old = 0.0f;
			unsigned int it = 0;

			while (likelihood_change > 0.1 && it < 100)
			{
				bool isFirstIt = false;
				if (it == 0)
					isFirstIt = true;
				kernel_expectation_step_1<<<dim3(n_blocks_em,N_CLUSTERS_INITIAL,1), nthreads_em>>>(d_tr, d_means, d_invCovs, d_weights, d_probs, d_irand, N_BINS, d_max, isFirstIt);
	
				float* d_likelihood;
				checkCudaErrors(cudaMalloc((void **) &d_likelihood, n_blocks_em * sizeof(float)));
				kernel_expectation_step_2<<<n_blocks_em, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_likelihood, d_max);
				checkCudaErrors(cudaMemcpy(likelihood, d_likelihood, n_blocks_em * sizeof(float), cudaMemcpyDeviceToHost));
				checkCudaErrors(cudaFree(d_likelihood));
				for (unsigned int j = 1; j < n_blocks_em; j++)
					likelihood[0] += likelihood[j];
				likelihood_change = abs(likelihood[0] - likelihood_old);
				likelihood_old = likelihood[0];

				kernel_maximization_step_n<<<N_CLUSTERS_INITIAL, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_n, d_weights, d_max);
				kernel_maximization_step_means<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_tr, d_probs, d_means, d_n, d_max);
				kernel_maximization_step_covariance<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_tr, d_probs, d_means, d_invCovs, d_n, d_max, INF, COV_MIN);
				it++;
			}
			*n_models += N_CLUSTERS_INITIAL;
			cout << "Number of EM Iterations :" << it << endl;
			cout << "Number of models :" << *n_models << endl;
		}
	}

	///// CLASSIFICATION STEPS /////
	kernel_classification_raw<<<blocks*5, 160>>>(d_hists, d_classified, d_means, d_invCovs, n_patches, N_CLUSTERS_INITIAL, THRESHOLD_CLASSIFY_MAH, INF);
	kernel_initLabels_ccl_improved<<<blocks*15,320>>>(d_classified, n_patches);
	m_IsNotDone = 1;
	ite = 0;
	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
	  kernel_ccl_mesh_B_improved_classify<<<dim3(WIDTH/SIZE_PATCH/16, HEIGHT/SIZE_PATCH/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_classified, d_roadlabel, d_maxgid, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      checkCudaErrors(cudaThreadSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	kernel_filterbiggestblob<<<blocks * 3, 320 >>>(d_classified, d_roadlabel, n_patches);
	
    // check if kernel execution generated and error
    getLastCudaError("Kernel execution failed");

	//get the classified image from the device
	checkCudaErrors(cudaMemcpy(classified, d_classified, mem_size_classified, cudaMemcpyDeviceToHost));
    
    checkCudaErrors(cudaEventRecord(stop, 0));
    // make sure GPU has finished copying
    checkCudaErrors(cudaDeviceSynchronize());
    //get the the total elapsed time in ms
    sdkStopTimer(&timer);
    checkCudaErrors(cudaEventElapsedTime(&elapsedTimeInMs, start, stop));
    elapsedTimeInMs = sdkGetTimerValue(&timer);

    sdkDeleteTimer(&timer);

	checkCudaErrors(cudaEventDestroy(stop));
    checkCudaErrors(cudaEventDestroy(start));
    
    return elapsedTimeInMs;
}




////////////////////////////////////////////////////////////////////////////////
//! Entry point for Cuda functionality on host side
//! @param argc  command line argument count
//! @param argv  command line arguments
//! @param data  data to process on the device
//! @param len   len of \a data
////////////////////////////////////////////////////////////////////////////////
extern "C" float
detectRoadFull(const int argc, const char **argv, float4 *data, unsigned int *hists, float* vars, unsigned int *ghist, unsigned int *cdf, unsigned int *tr, uint4 *min, uint2 *loc, unsigned int *trlabels, unsigned int *trsize, unsigned int *max, unsigned int *maxgid, unsigned int *trcompact, unsigned int *ntr, float *probs, unsigned int *iRand, float *likelihood, float *n, float *means, float *invCovs, float *logconsts, unsigned int *classified, unsigned int *roadlabel)
{
	//number of pixels
	unsigned int n_pix = WIDTH * HEIGHT;
	//number of 4-element histogram pieces
	unsigned int n_hists = n_pix / SIZE_PATCH / SIZE_PATCH * N_BINS_SQR;
	//number of patches
	unsigned int n_patches = n_pix / SIZE_PATCH / SIZE_PATCH;
	StopWatchInterface *timer = NULL;
    float elapsedTimeInMs = 0.0f;
    cudaEvent_t start, stop;
    sdkCreateTimer(&timer);
    checkCudaErrors(cudaEventCreate(&start));
    checkCudaErrors(cudaEventCreate(&stop));

    // use command-line specified CUDA device, otherwise use device with highest Gflops/s
    findCudaDevice(argc, (const char **)argv);
	// calculate the required size to allocate from device memory
    const unsigned int mem_size_data = sizeof(float4) * n_pix;
    const unsigned int mem_size_hists = sizeof(unsigned int) * n_hists;
	const unsigned int mem_size_vars = sizeof(float) * n_patches;
	const unsigned int mem_size_ghist = sizeof(unsigned int) * 256 * 3;
	const unsigned int mem_size_cdf = sizeof(unsigned int) * 256 * 3;
	//const unsigned int mem_size_loc = sizeof(uint2) * n_patches;
	//const unsigned int mem_size_trbinary = sizeof(unsigned char) * n_patches;
	const unsigned int mem_size_trlabels = sizeof(unsigned int) * n_patches;
	const unsigned int mem_size_trsize = sizeof(unsigned int) * n_patches;
	const unsigned int mem_size_trcompact = sizeof(unsigned int) * n_patches;
	const unsigned int mem_size_classified = sizeof(unsigned int) * n_patches;
	const unsigned int mem_size_means = sizeof(float) * N_BINS_SQR * N_CLUSTERS_MAX;
	const unsigned int mem_size_invCovs = sizeof(float) * N_BINS_SQR * N_CLUSTERS_MAX;
	const unsigned int mem_size_weights = sizeof(float) * N_CLUSTERS_MAX;
	const unsigned int mem_size_probs = sizeof(float) * N_CLUSTERS_MAX * n_patches;
	//const unsigned int mem_size_logconsts = sizeof(float) * N_MODELS_MAX;
	const unsigned int mem_size_n = sizeof(float) * N_CLUSTERS_MAX;
	const unsigned int mem_size_irand = sizeof(unsigned int) * N_CLUSTERS_MAX;
	

    // allocate device memory
    float4 *d_data;
    unsigned int *d_hists;
	float *d_vars;
	unsigned int *d_ghist;	//3 channel histogram of the image, each channel has 256 bins
	unsigned int *d_cdf;	//cumulative distribution function
	unsigned int *d_max;	//maximum number of training samples in a blob
	unsigned int *d_maxgid; //memory location of the training sample with min label in the biggest blob
	unsigned int *d_tr;	//training samples
	uint4 *d_min;
	//uint2 *d_loc;	//location of the training samples
	//unsigned char *d_trbinary;		//a binary image that holds the location of the training samples (1 if training sample, 0 if not)
	unsigned int *d_trlabels;
	unsigned int *d_trsize;
	unsigned int *d_trcompact;		//the array that stores the compaction indices of the training samples
	unsigned int *d_ntr; //number of training samples
	unsigned int *d_classified;	//classified downsampled image 
	unsigned int *d_roadlabel; //label of road pixels found by region growing
	
	////Gaussian Models////
	float *d_means;
	float *d_invCovs;
	float *d_weights;
	//float *d_logconsts;
	float *d_probs;
	float *d_n;
	unsigned int *d_irand;

    checkCudaErrors(cudaMalloc((void **) &d_data, mem_size_data));
    checkCudaErrors(cudaMalloc((void **) &d_hists, mem_size_hists));
	checkCudaErrors(cudaMalloc((void **) &d_vars, mem_size_vars));
	checkCudaErrors(cudaMalloc((void **) &d_ghist, mem_size_ghist));
	checkCudaErrors(cudaMalloc((void **) &d_cdf, mem_size_cdf));
	checkCudaErrors(cudaMalloc((void **) &d_tr, mem_size_hists));
	checkCudaErrors(cudaMalloc((void **) &d_min, sizeof(uint4)));
	//checkCudaErrors(cudaMalloc((void **) &d_loc, mem_size_loc));
	//checkCudaErrors(cudaMalloc((void **) &d_trbinary, mem_size_trbinary));
	checkCudaErrors(cudaMalloc((void **) &d_trcompact, mem_size_trcompact));
	checkCudaErrors(cudaMalloc((void **) &d_trlabels, mem_size_trlabels));
	checkCudaErrors(cudaMalloc((void **) &d_trsize, mem_size_trsize));
	checkCudaErrors(cudaMalloc((void **) &d_classified, mem_size_classified));
	checkCudaErrors(cudaMalloc((void **) &d_roadlabel, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_max, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_maxgid, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_ntr, sizeof(unsigned int)));
	checkCudaErrors(cudaMalloc((void **) &d_means, mem_size_means));
	checkCudaErrors(cudaMalloc((void **) &d_invCovs, mem_size_invCovs));
	checkCudaErrors(cudaMalloc((void **) &d_weights, mem_size_weights));
	checkCudaErrors(cudaMalloc((void **) &d_probs, mem_size_probs));
	//checkCudaErrors(cudaMalloc((void **) &d_logconsts, mem_size_logconsts));
	checkCudaErrors(cudaMalloc((void **) &d_n, mem_size_n));
	checkCudaErrors(cudaMalloc((void **) &d_irand, mem_size_irand));


	checkCudaErrors(cudaMemset( d_ghist, 0, mem_size_ghist));
	checkCudaErrors(cudaMemset( d_cdf, 0, mem_size_cdf));
	//checkCudaErrors(cudaMemset( d_trbinary, 0, mem_size_trbinary));
	checkCudaErrors(cudaMemset( d_trlabels, 0, mem_size_trlabels));
	checkCudaErrors(cudaMemset( d_trsize, 0, mem_size_trsize));
	//checkCudaErrors(cudaMemset( d_classified, 0, mem_size_classified));
	checkCudaErrors(cudaMemset( d_ntr, 0, sizeof(unsigned int)));
	//checkCudaErrors(cudaMemset( d_invCovs, 1.0f, mem_size_invCovs));
	//checkCudaErrors(cudaMemset( d_weights, 1.0f/N_MODELS_LEARNED, mem_size_weights));
	//float pwr = N_BINS_SQR / 2;
	//float cnst = pow(2.0f*(float)M_PI, pwr);
	//checkCudaErrors(cudaMemset( d_consts, cnst, mem_size_consts ));
    
	////// CCL STUFF //////
	unsigned int *m_d_IsNotDone;
	checkCudaErrors(cudaMalloc((void **) &m_d_IsNotDone, sizeof(unsigned int)));
	unsigned int m_IsNotDone = 1;
	unsigned int ite = 0;

    //start the timer
    sdkStartTimer(&timer);
    checkCudaErrors(cudaEventRecord(start, 0));
    
    // copy image data to device
    checkCudaErrors(cudaMemcpy(d_data, data, mem_size_data, cudaMemcpyHostToDevice));

	//bind data to the texture
	cudaBindTexture( 0, texImg, d_data, mem_size_data );
	cudaBindTexture( 0, texCdf, d_cdf, mem_size_cdf );
	//cudaBindTexture( 0, texTrBinary, d_trbinary, mem_size_trbinary );
	cudaBindTexture( 0, texTrLabels, d_trlabels, mem_size_trlabels );
	cudaBindTexture( 0, texTrSize, d_trsize, mem_size_trsize );
	cudaBindTexture( 0, texTrCompact, d_trcompact, mem_size_trcompact );
	cudaBindTexture( 0, texClassified, d_classified, mem_size_classified );

	cudaDeviceProp prop;
	checkCudaErrors( cudaGetDeviceProperties( &prop, 0 ) );
	int blocks = prop.multiProcessorCount;
    
	//////////////// ALGORITHM BEGINS ////////////////

	///////////////// PRE-PROCESSING //////////////////
	kernel_hist_tex<<< blocks*4, 256 >>>(d_ghist, n_pix);
	kernel_prefixsumscan_cdf<<< 1, 256/2>>>(d_cdf, d_ghist, 256, n_pix);
	kernel_findmin<<<1, 256>>>(d_cdf, d_min);
	kernel_histeq<<< blocks*4, 256 >>>(d_data, d_min, n_pix);
	//kernel_rgb2hsv<<< blocks*15, 320 >>>(d_data, width*height);
	kernel_rgb2hsv_2<<< blocks*15, 320 >>>(d_data, n_pix);
	//kernel_downsample<<< blocks*16, dim3(size_patch, size_patch, 1), n_bins * n_bins * size_patch * size_patch * sizeof(unsigned char) >>>(d_data, d_hists, d_vars, width, width/size_patch, height, height/size_patch, (width/size_patch)*(height/size_patch), n_bins, size_patch);
	//kernel_downsample_2<<< blocks*16, dim3(4*size_patch, size_patch, 1), 4 * n_bins * n_bins * size_patch * size_patch * sizeof(unsigned char) >>>(d_data, d_hists, d_vars, width, (width/size_patch)*(height/size_patch)/4, n_bins, size_patch);
	//kernel_downsample_3<<< blocks*20, dim3(4*size_patch, size_patch, 1), 4 * n_bins * n_bins * size_patch * size_patch * sizeof(unsigned char) >>>(d_data, d_hists, d_vars, width, (width/size_patch)*(height/size_patch)/4, size_patch);
	//kernel_downsample_4<<< blocks*15, 320 >>>(d_data, d_hists, d_vars, width, (width/size_patch)*(height/size_patch), size_patch);
	//kernel_downsample_4_trlocs<<< blocks*15, 320 >>>(d_data, d_hists, d_trbinary, d_ntr, width, (width/size_patch)*(height/size_patch), size_patch, THRESHOLD_VAR_Y);
	kernel_downsample_4_trlocs_uint<<< blocks*15, 320 >>>(d_data, d_hists, d_trlabels, d_trsize, d_ntr, WIDTH, n_patches, SIZE_PATCH, THRESHOLD_VAR_Y);
	//kernel_downsample_4_trlocs_uint<<< blocks*15, 320 >>>(d_data, d_hists, d_trsize, d_ntr, width, (width/size_patch)*(height/size_patch), size_patch, THRESHOLD_VAR_Y);
	//kernel_prefixsumscan_compact<<< 1, 512 >>>(d_trcompact, d_trbinary, 1024, width*height);

	
	//dim3 bl = make_uint3(256, 1, 1);
	//int gs = SIZEX % 16 ? SIZEX / 16 + 1 : SIZEX / 16;
	//dim3 gd = make_uint3(gs, gs, 1);


	///////////// CONNECTED COMPONENTS LABELING ///////////////////
	//kernel_initLabels_ccl<<<n_patches/3/64,64*3>>>(d_trlabels);
	kernel_initLabels_ccl_improved<<<blocks*15,320>>>(d_trlabels, n_patches);
	////kernel_initLabels_ccl_improved1<<<n_patches/3/64,64*3>>>(d_trsize);
	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
      //m_IsNotDone = 0;
      //checkCudaErrors(cudaMemcpy(m_d_IsNotDone, &m_IsNotDone, sizeof(unsigned int), cudaMemcpyHostToDevice));
      //checkCudaErrors(cudaThreadSynchronize());
	  //kernel_ccl_mesh_A<<<n_patches/3/64,64*3>>>(d_trbinary, d_trlabels, m_d_IsNotDone, width/size_patch, height/size_patch);
	  //kernel_ccl_mesh_B<<<dim3(width/size_patch/16, height/size_patch/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_trbinary, d_trlabels, m_d_IsNotDone, width/size_patch, height/size_patch);
	  kernel_ccl_mesh_B_improved<<<dim3(WIDTH/SIZE_PATCH/16, HEIGHT/SIZE_PATCH/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_trlabels, d_trsize, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      //kernel_scanning<<<gd,bl>>>(d_trlabels, m_d_IsNotDone);
      checkCudaErrors(cudaThreadSynchronize());
      //kernel_analysis<<<gd,bl>>>(d_trlabels, d_trsize);
      //checkCudaErrors(cudaThreadSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	kernel_findmax<<< 1, 64 >>>(d_trsize, d_max, d_maxgid, n_patches);
	kernel_filterbiggestblob<<<blocks * 3, 320 >>>(d_trlabels, d_maxgid, n_patches);
	//2*blocksize must be a power of two and n_patches should be dividable to 2*blocksize
	kernel_prefixsumscan_compact_uint<<< 1, 32, 64 * sizeof(unsigned int) >>>(d_trcompact, d_trlabels, n_patches);
	//allocate resultant training samples
	kernel_insert_training_samples<<< blocks * 3, 320>>>(d_trlabels, d_trcompact, d_hists, d_tr, d_max, n_patches);

	/// EXPECTATION MAXIMIZATION ///
	checkCudaErrors(cudaMemcpy(ntr, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	
	if (ntr > 0)
	{
		insertRandIndices(iRand, ntr);
		checkCudaErrors(cudaMemcpy(d_irand, iRand, mem_size_irand, cudaMemcpyHostToDevice));
		//unsigned int tst1 = *ntr%512;
	
		int pow = 0, pow_n_bins_sqr = 0;
		for (int i = *ntr; i > 1; i>>=1)
			pow++;
		for (int i = N_BINS_SQR; i > 1; i>>=1)
			pow_n_bins_sqr++;
		unsigned int nthreads_em = 1 << pow;
		unsigned int nthreads_em_cnsts = 1 << pow_n_bins_sqr;
		nthreads_em = (nthreads_em > 256) ? 256 : nthreads_em;
		nthreads_em_cnsts = (nthreads_em_cnsts > 256) ? 256 : nthreads_em_cnsts;
		unsigned int n_blocks_em = 2;

		float likelihood_change = INF;
		float likelihood_old = 0.0f;
		//float likelihood_curr = 0.0f;
		unsigned int it = 0;

		//for (int i = 0; i < 100; i++)
		while (likelihood_change > 0.1 && it < 100)
		{
			bool isFirstIt = false;
			if (it == 0)
				isFirstIt = true;
			kernel_expectation_step_1<<<dim3(n_blocks_em,N_CLUSTERS_INITIAL,1), nthreads_em>>>(d_tr, d_means, d_invCovs, d_weights, d_probs, d_irand, N_BINS, d_max, isFirstIt);
	
			float* d_likelihood;
			checkCudaErrors(cudaMalloc((void **) &d_likelihood, n_blocks_em * sizeof(float)));
			kernel_expectation_step_2<<<n_blocks_em, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_likelihood, d_max);
			checkCudaErrors(cudaMemcpy(likelihood, d_likelihood, n_blocks_em * sizeof(float), cudaMemcpyDeviceToHost));
			checkCudaErrors(cudaFree(d_likelihood));
			for (unsigned int j = 1; j < n_blocks_em; j++)
				likelihood[0] += likelihood[j];
			likelihood_change = abs(likelihood[0] - likelihood_old);
			likelihood_old = likelihood[0];
			//cout << "LIKELIHOOD " << i << ": " << likelihood[0] << endl;

			kernel_maximization_step_n<<<N_CLUSTERS_INITIAL, nthreads_em, nthreads_em * sizeof(float)>>>(d_probs, d_n, d_weights, d_max);
			kernel_maximization_step_means<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_tr, d_probs, d_means, d_n, d_max);
			kernel_maximization_step_covariance<<<dim3(N_CLUSTERS_INITIAL, N_BINS_SQR, 1), nthreads_em, nthreads_em * sizeof(float)>>>(d_tr, d_probs, d_means, d_invCovs, d_n, d_max, INF, COV_MIN);
			//kernel_maximization_step_logconsts<<<N_MODELS_LEARNED, nthreads_em_cnsts, nthreads_em_cnsts * sizeof(float)>>>(d_invCovs, d_logconsts);
			//checkCudaErrors(cudaThreadSynchronize());
			it++;
		}
		cout << "Number of EM Iterations :" << it << endl;
	}

	///// CLASSIFICATION STEPS /////
	//checkCudaErrors(cudaMemset( d_trsize, 0, mem_size_trsize));
	kernel_classification_raw<<<blocks*5, 160>>>(d_hists, d_classified, d_means, d_invCovs, n_patches, N_CLUSTERS_INITIAL, THRESHOLD_CLASSIFY_MAH, INF);
	//kernel_classification_raw<<<blocks*5, 160>>>(d_hists, d_trsize, d_means, d_invCovs, (width/size_patch)*(height/size_patch), N_MODELS_LEARNED, 16, INF);

	
	//kernel_initLabels_ccl<<<n_patches/3/64,64*3>>>(d_trlabels);
	kernel_initLabels_ccl_improved<<<blocks*15,320>>>(d_classified, n_patches);
	////kernel_initLabels_ccl_improved1<<<n_patches/3/64,64*3>>>(d_trsize);
	m_IsNotDone = 1;
	ite = 0;

	
	while(m_IsNotDone)
    {
	  checkCudaErrors(cudaMemset( m_d_IsNotDone, 0, sizeof(unsigned int)));
      //m_IsNotDone = 0;
      //checkCudaErrors(cudaMemcpy(m_d_IsNotDone, &m_IsNotDone, sizeof(unsigned int), cudaMemcpyHostToDevice));
      //checkCudaErrors(cudaThreadSynchronize());
	  //kernel_ccl_mesh_A<<<n_patches/3/64,64*3>>>(d_trbinary, d_trlabels, m_d_IsNotDone, width/size_patch, height/size_patch);
	  //kernel_ccl_mesh_B<<<dim3(width/size_patch/16, height/size_patch/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_trbinary, d_trlabels, m_d_IsNotDone, width/size_patch, height/size_patch);
	  kernel_ccl_mesh_B_improved_classify<<<dim3(WIDTH/SIZE_PATCH/16, HEIGHT/SIZE_PATCH/30, 1), dim3(16,30,1), 16*30*sizeof(unsigned int)>>>(d_classified, d_roadlabel, d_maxgid, m_d_IsNotDone, WIDTH/SIZE_PATCH, HEIGHT/SIZE_PATCH);
      //kernel_scanning<<<gd,bl>>>(d_trlabels, m_d_IsNotDone);
      checkCudaErrors(cudaThreadSynchronize());
      //kernel_analysis<<<gd,bl>>>(d_trlabels, d_trsize);
      //checkCudaErrors(cudaThreadSynchronize());
      checkCudaErrors(cudaMemcpy(&m_IsNotDone, m_d_IsNotDone, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	  ite++;
    }

	kernel_filterbiggestblob<<<blocks * 3, 320 >>>(d_classified, d_roadlabel, n_patches);
	
	
	cout << "Number of Classification Iterations :" << ite << endl;

	//kernel_histeq<<< blocks*4, 256 >>>(d_data, d_ghist, 640*480);
	//kernel_histeq_npixelsperthread2<<< grid_histeq2, threads_histeq2 >>>(d_data, d_ghist);
    //kernel_naive_2<<< grid, threads, n_bins * n_bins * size_patch * size_patch * sizeof(unsigned char) >>>(d_data, d_hists, d_vars, size_patch, n_bins);
    //kernel_naive<<< grid, threads >>>(d_data, d_hists, size_patch, n_bins);

	
    // check if kernel execution generated and error
    getLastCudaError("Kernel execution failed");

    // copy results from device to host
    //checkCudaErrors(cudaMemcpy(data, d_data, mem_size_data, cudaMemcpyDeviceToHost));
    //checkCudaErrors(cudaMemcpy(hists, d_hists, mem_size_hists, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(vars, d_vars, mem_size_vars, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(ghist, d_ghist, mem_size_ghist, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(cdf, d_cdf, mem_size_cdf, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(tr, d_tr, mem_size_hists, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(min, d_min, sizeof(uint4), cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(loc, d_loc, mem_size_loc, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(trbinary, d_trbinary, mem_size_trbinary, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(trlabels, d_trlabels, mem_size_trlabels, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(trsize, d_trsize, mem_size_trsize, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(max, d_max, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(maxgid, d_maxgid, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(trcompact, d_trcompact, mem_size_trcompact, cudaMemcpyDeviceToHost));
	checkCudaErrors(cudaMemcpy(classified, d_classified, mem_size_classified, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(roadlabel, d_roadlabel, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(ntr, d_ntr, sizeof(unsigned int), cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(probs, d_probs, mem_size_probs, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(n, d_n, mem_size_n, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(means, d_means, mem_size_means, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(invCovs, d_invCovs, mem_size_invCovs, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(logconsts, d_logconsts, mem_size_logconsts, cudaMemcpyDeviceToHost));
	//checkCudaErrors(cudaMemcpy(iRand, d_irand, mem_size_irand, cudaMemcpyDeviceToHost));

	//unbind the texture
	cudaUnbindTexture(texImg);
	cudaUnbindTexture(texCdf);
	//cudaUnbindTexture(texTrBinary);
	cudaUnbindTexture(texTrLabels);
	cudaUnbindTexture(texTrSize);
	cudaUnbindTexture(texTrCompact);
	cudaUnbindTexture(texClassified);
    
    checkCudaErrors(cudaEventRecord(stop, 0));
    // make sure GPU has finished copying
    checkCudaErrors(cudaDeviceSynchronize());
    //get the the total elapsed time in ms
    sdkStopTimer(&timer);
    checkCudaErrors(cudaEventElapsedTime(&elapsedTimeInMs, start, stop));
    elapsedTimeInMs = sdkGetTimerValue(&timer);

    // cleanup memory
    checkCudaErrors(cudaFree(d_data));
    checkCudaErrors(cudaFree(d_hists));
	checkCudaErrors(cudaFree(d_vars));
	checkCudaErrors(cudaFree(d_ghist));
	checkCudaErrors(cudaFree(d_cdf));
	checkCudaErrors(cudaFree(d_tr));
	checkCudaErrors(cudaFree(d_min));
	//checkCudaErrors(cudaFree(d_loc));
	//checkCudaErrors(cudaFree(d_trbinary));
	checkCudaErrors(cudaFree(d_trlabels));
	checkCudaErrors(cudaFree(d_trsize));
	checkCudaErrors(cudaFree(d_max));
	checkCudaErrors(cudaFree(d_maxgid));
	checkCudaErrors(cudaFree(d_trcompact));
	checkCudaErrors(cudaFree(d_classified));
	checkCudaErrors(cudaFree(d_roadlabel));
	checkCudaErrors(cudaFree(d_ntr));
	checkCudaErrors(cudaFree(d_means));
	checkCudaErrors(cudaFree(d_invCovs));
	checkCudaErrors(cudaFree(d_weights));
	checkCudaErrors(cudaFree(d_probs));
	//checkCudaErrors(cudaFree(d_logconsts));
	checkCudaErrors(cudaFree(d_n));
	checkCudaErrors(cudaFree(d_irand));
    checkCudaErrors(cudaEventDestroy(stop));
    checkCudaErrors(cudaEventDestroy(start));
    sdkDeleteTimer(&timer);
    
    return elapsedTimeInMs;
	//return float(ite);
}
*/