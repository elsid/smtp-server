#ifndef SMTP_SERVER_TRANSITION_RESULT
#define SMTP_SERVER_TRANSITION_RESULT

typedef enum transaction_result {
    TRANSITION_SUCCEED,
    TRANSITION_WAIT,
    TRANSITION_ERROR,
    TRANSITION_FAILED,
} transition_result_t;

#endif
