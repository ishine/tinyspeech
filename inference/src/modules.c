#include "modules.h"

Tensor batchnorm2d(Tensor* input, Tensor* mean, Tensor* variance, Tensor* gamma, Tensor* beta) {
    int8_t N = input->shape[0];
    int8_t C = input->shape[1];
    int8_t H = input->shape[2];
    int8_t W = input->shape[3];

    int8_t shape[4] = {N, C, H, W};
    Tensor output = create_tensor(shape, 4);

    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            float var_sqrt = sqrtf(variance->f_data[c] + 0.0001);
            for (int h = 0; h < H; h++) {
                for (int w = 0; w < W; w++) {
                    int idx = n * (C * H * W) + c * (H * W) + h * W + w;
                    float scaled = gamma->f_data[c] * ((float)input->data[idx] - mean->f_data[c]) / var_sqrt + beta->f_data[c];
                    output.data[idx] = (int8_t)roundf(scaled);
                }
            }
        }
    }

    return output; 
}

Tensor conv2d(Tensor *input, Tensor *weights, Tensor *bias, int stride, int padding) {
    int batch_size = input->shape[0];
    int in_channels = input->shape[1];
    int in_height = input->shape[2];
    int in_width = input->shape[3];

    int out_channels = weights->shape[0];
    int kernel_height = weights->shape[2];
    int kernel_width = weights->shape[3];

    int out_height = (in_height + 2 * padding - kernel_height) / stride + 1;
    int out_width = (in_width + 2 * padding - kernel_width) / stride + 1;

    int8_t shape[4] = {batch_size, out_channels, out_height, out_width};
    Tensor output = create_tensor(shape, 4);

    for (int n = 0; n < batch_size; n++) {
        for (int oc = 0; oc < out_channels; oc++) {
            for (int h = 0; h < out_height; h++) {
                for (int w = 0; w < out_width; w++) {
                    int32_t sum = 0;
                    for (int ic = 0; ic < in_channels; ic++) {
                        for (int kh = 0; kh < kernel_height; kh++) {
                            for (int kw = 0; kw < kernel_width; kw++) {
                                int ih = h * stride + kh - padding;
                                int iw = w * stride + kw - padding;

                                if (ih >= 0 && ih < in_height && iw >= 0 && iw < in_width) {
                                    int in_index = n * (in_channels * in_height * in_width) +
                                                   ic * (in_height * in_width) +
                                                   ih * in_width + iw;

                                    int weight_index = oc * (in_channels * kernel_height * kernel_width) +
                                                       ic * (kernel_height * kernel_width) +
                                                       kh * kernel_width + kw;

                                    sum += input->data[in_index] * weights->data[weight_index];
                                }
                            }
                        }
                    }
                    sum += bias->data[oc];
                    int out_index = n * (out_channels * out_height * out_width) +
                                    oc * (out_height * out_width) +
                                    h * out_width + w;

                    output.data[out_index] = (int8_t)(sum/100);
                }
            }
        }
    }
    return output;
}   

Tensor fc_layer(Tensor *input, Tensor *weights) {
    int8_t batch_size = input->shape[0];
    int8_t input_features = input->shape[1];
    int8_t output_features = weights->shape[0];

    int8_t shape[2] = {batch_size, output_features};
    Tensor output = f_create_tensor(shape, 2);

    for (uint8_t n = 0; n < batch_size; n++) {
        for (uint8_t o = 0; o < output_features; o++) {
            float sum = 0.0f;
            for (int i = 0; i < input_features; i++) {
                sum += input->f_data[n * input_features + i] * weights->f_data[o * input_features + i];
            }
            output.f_data[n * output_features + o] = sum;
        }
    }
    return output; 
}


Tensor maxpool2d(Tensor* input, int kernel_size, int stride) {
    int8_t shape[4] =  {input->shape[0], input->shape[1], ((input->shape[2] - kernel_size) / stride + 1), ((input->shape[3] - kernel_size) / stride + 1)};
    Tensor output = create_tensor(shape, 4);

    for (int b = 0; b < output.shape[0]; b++) { // Batch 
        for (int c = 0; c < output.shape[1]; c++) { // Channel 
            for (int oh = 0; oh < output.shape[2]; oh++) { // Output height
                for (int ow = 0; ow < output.shape[3]; ow++) { // Output width
                    int max_value = INT8_MIN;

                    for (int kh = 0; kh < kernel_size; kh++) { // Kernel height
                        for (int kw = 0; kw < kernel_size; kw++) { // Kernel width
                            int ih = oh * stride + kh; 
                            int iw = ow * stride + kw; 

                            int input_index = b * (input->shape[1] * input->shape[2] * input->shape[3]) +
                                              c * (input->shape[2] * input->shape[3]) +
                                              ih * input->shape[3] +
                                              iw;

                            if (input->data[input_index] > max_value) {
                                max_value = input->data[input_index];
                            }
                        }
                    }

                    int output_index = b * (output.shape[1] * output.shape[2] * output.shape[3]) +
                                       c * (output.shape[2] * output.shape[3]) +
                                       oh * output.shape[3] +
                                       ow;

                    output.data[output_index] = max_value;
                }
            }
        }
    }

    return output;
}

Tensor softmax(Tensor *input) {
    int batch_size = input->shape[0];
    int num_classes = input->shape[1];

    int8_t shape[2] = {batch_size, num_classes};
    Tensor output = f_create_tensor(shape, 2);

    for (int n = 0; n < batch_size; n++) {
        // Find max value for numerical stability
        float max_val = -FLT_MAX;
        for (int c = 0; c < num_classes; c++) {
            int index = n * num_classes + c;
            if (input->data[index] > max_val) {
                max_val = input->data[index];
            }
        }

        // Compute exponentials and their sum
        float sum_exp = 0.0f;
        for (int c = 0; c < num_classes; c++) {
            int index = n * num_classes + c;
            output.f_data[index] = expf(input->data[index] - max_val);
            sum_exp += output.f_data[index];
        }

        // Normalize to get probabilities
        for (int c = 0; c < num_classes; c++) {
            int index = n * num_classes + c;
            output.f_data[index] /= sum_exp;
        }
    }

    return output; 
}

Tensor upsample_nearest(Tensor* input, int in_size, int8_t scale_factor) {

    int8_t shape[4] = {input->shape[0], input->shape[1], input->shape[2] * scale_factor, input->shape[3] * scale_factor};
    Tensor output = create_tensor(shape, 4); 

    if (!output.data) {
        perror("Memory allocation failed for output.data");
        exit(EXIT_FAILURE);
    }

    for (int b = 0; b < output.shape[0]; b++) { // Batch 
        for (int c = 0; c < output.shape[1]; c++) { // Channel 
            for (int h = 0; h < output.shape[2]; h++) { // Height 
                int nearest_h = h / scale_factor;
                for (int w = 0; w < output.shape[3]; w++) { // Width 
                    int nearest_w = w / scale_factor;
                    int input_index = b * (input->shape[1] * input->shape[2] * input->shape[3]) +
                                      c * (input->shape[2] * input->shape[3]) +
                                      nearest_h * input->shape[3] +
                                      nearest_w;

                    int output_index = b * (output.shape[1] * output.shape[2] * output.shape[3]) +
                                       c * (output.shape[2] * output.shape[3]) +
                                       h * output.shape[3] +
                                       w;

                    output.data[output_index] = input->data[input_index];
                }
            }
        }
    }
    fprintf(stdout, "Loaded tensor with shape [%d, %d, %d, %d]\n", output.shape[0], output.shape[1], output.shape[2], output.shape[3]);
    return output;
}


Tensor AttentionCondenser(Tensor* input, int8_t in_channels, int8_t mid_channels, int8_t out_channels) { 

    Tensor residual = input; // fix 
    Tensor Q = maxpool2d(input, 2, 2);

    Tensor V_prime = create_tensor(input.shape, 4); 
    


}

Tensor Attn_BN_Block(Tensor* input, int8_t in_channels, int8_t mid_channels_0, int8_t out_channels_0, int8_t mid_channels_1, int8_t out_channels_1)  { 

}